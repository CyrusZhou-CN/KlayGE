import argparse
import os
import math
import struct
import time

import torch
import torch.nn as nn
import torch.optim as optim

class IntegratedBrdfDataset(torch.utils.data.Dataset):
	def __init__(self, file_name):
		with open(file_name, "rb") as file:
			sample_count = struct.unpack("<I", file.read(4))[0]
			self.coord_data = torch.empty(sample_count, 2)
			self.x_data = torch.empty(sample_count)
			self.y_data = torch.empty(sample_count)
			for i in range(sample_count):
				self.coord_data[i] = torch.tensor(struct.unpack("<ff", file.read(8)))
				self.x_data[i] = struct.unpack("<f", file.read(4))[0]
				self.y_data[i] = struct.unpack("<f", file.read(4))[0]

			self.output_data = self.x_data

	def XChannelMode(self, x_mode):
		self.output_data = self.x_data if x_mode else self.y_data

	def __len__(self):
		return len(self.coord_data)

	def __getitem__(self, index):
		return self.coord_data[index], self.output_data[index]

class IntegratedBrdfNetwork(nn.Module):
	def __init__(self, num_features):
		super(IntegratedBrdfNetwork, self).__init__()

		layers = []
		for i in range(len(num_features) - 1):
			layers.append(nn.Linear(num_features[i], num_features[i + 1]))
			layers.append(nn.SELU())
		
		self.net = nn.Sequential(*layers)

	def forward(self, x):
		return self.net(x).squeeze(1)

	def Write(self, file, var_name):
		file.write(f"/*{var_name} structure: {self}*/\n\n")

		for i in range((len(self.net) + 1) // 2):
			weight = self.net[i * 2].weight.data
			dim = weight.size()[1]
			file.write(f"float const {var_name}_layer_{i + 1}_weight[][{dim}] = \n{{\n")
			for row_count, row in enumerate(weight):
				file.write("\t{")
				for col_count, col in enumerate(row):
					file.write(f"{col:.9f}f")
					if (row_count != len(weight) - 1) or (col_count != len(row) - 1):
						if col_count != len(row) - 1:
							file.write(", ")
				file.write("},\n")
			file.write("};\n")

			bias = self.net[i * 2].bias.data
			dim = bias.size()[0]
			file.write(f"float const {var_name}_layer_{i + 1}_bias[]{{")
			for col_count, col in enumerate(bias):
				file.write(f"{col:.9f}f")
				if col_count != len(bias) - 1:
					file.write(", ")
			file.write("};\n\n")

class IntegratedBrdfExpression(nn.Module):
	def __init__(self, order, init_factors = None):
		super(IntegratedBrdfExpression, self).__init__()

		self.order = order
		n = self.order + 1

		num_factors = n * n
		if init_factors == None:
			init_factors = torch.distributions.Uniform(0, 5).sample((num_factors, ))
		else:
			init_factors = init_factors.t().reshape(num_factors)

		self.weights = nn.Parameter(init_factors)

	def forward(self, x):
		n_dot_v, glossiness = torch.tensor_split(x, 2, dim = 1)
		weights = self.weights
		n = self.order + 1
		dim_x = []
		for y in range(n):
			tmp = weights[y * n]
			for x in range(n - 1):
				tmp = tmp * glossiness + weights[y * n + x + 1]
			dim_x.append(tmp)
		dim_y = dim_x[0]
		for x in range(n - 1):
			dim_y = dim_y * n_dot_v + dim_x[x + 1]
		return dim_y.squeeze(1)

	def Write(self, file, var_name):
		n = self.order + 1
		weights = self.weights
		file.write(f"float{n} const {var_name}[] = {{\n")
		for y in range(n):
			file.write("\t{")
			for x in range(n):
				file.write(f"{weights[x * n + y]:.9f}f")
				if (x != n - 1):
					file.write(", ")
			file.write("},\n")
		file.write("};\n")

class ModelDesc:
	def __init__(self, name, model_class, model_param, x_channel_mode, output_file_name):
		self.name = name
		self.model_class = model_class
		self.model_param = model_param
		self.x_channel_mode = x_channel_mode
		self.output_file_name = output_file_name

def TrainModel(data_set, model, batch_size, learning_rate, epochs, output_file_name, var_name):
	train_loader = torch.utils.data.DataLoader(data_set, batch_size = batch_size, shuffle = True)

	device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
	model.to(device)

	criterion = nn.MSELoss()
	optimizer = optim.Adam(model.parameters(), lr = learning_rate)
	scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, "min", factor = 0.5, verbose = True)

	start = time.time()
	min_loss = 1e10
	for epoch in range(epochs):
		running_loss = 0.0
		for data in train_loader:
			inputs = data[0].to(device)
			targets = data[1].to(device)

			optimizer.zero_grad()

			outputs = model(inputs)
			loss = criterion(outputs, targets)
			loss.backward()
			optimizer.step()

			running_loss += loss.item()
		loss = running_loss / len(train_loader)
		scheduler.step(loss)
		if loss < min_loss:
			with open(output_file_name, "w") as file:
				model.Write(file, var_name)
				file.write(f"// [{epoch + 1}] Loss: {loss}\n")
			min_loss = loss
		print(f"[{epoch + 1}] Loss: {loss}")
		if loss < 1e-7:
			break
	timespan = time.time() - start

	print(f"Finished training in {(timespan / 60):.2f} mins.")

def TestModel(testing_set, model, batch_size):
	test_loader = torch.utils.data.DataLoader(testing_set, batch_size = batch_size, shuffle = False)

	device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
	model.to(device)

	total_mse = 0.0
	total_count = 0
	with torch.no_grad():
		for data in test_loader:
			inputs = data[0].to(device)
			targets = data[1].to(device)

			outputs = model(inputs)
			diff = outputs - targets
			total_mse += torch.sum(diff * diff).item()
			total_count += targets.size(0)

	return total_mse / total_count

def TrainModels(training_set, testing_set, batch_size, learning_rate, epochs, model_descs):
	models = []
	for model_desc in model_descs:
		training_set.XChannelMode(model_desc.x_channel_mode)
		model = model_desc.model_class(model_desc.model_param)
		TrainModel(training_set, model, batch_size, learning_rate, epochs, model_desc.output_file_name, model_desc.name)
		models.append(model)

	for model, model_desc in zip(models, model_descs):
		testing_set.XChannelMode(model_desc.x_channel_mode)
		model.eval()
		mse = TestModel(testing_set, model, batch_size)
		print(f"MSE of the {model_desc.name} on the {len(testing_set)} test samples: {mse}")

def ParseCommandLine():
	parser = argparse.ArgumentParser()

	parser.add_argument("--batch-size", dest = "batch_size", default = 1024, type = int, help = "batch size in training")
	parser.add_argument("--learning-rate", dest = "learning_rate", default = 0.01, type = float, help = "epochs in training")
	parser.add_argument("--epochs", dest = "epochs", default = 500, type = int, help = "epochs in training")

	return parser.parse_args()

if __name__ == "__main__":
	args = ParseCommandLine()

	print("Loading training set...")
	training_set = IntegratedBrdfDataset("IntegratedBRDF_1048576.dat")

	print("Loading testing set...")
	testing_set = IntegratedBrdfDataset("IntegratedBRDF_4096.dat")

	model_descs = (
		(
			"4-Layer NN",
			(
				ModelDesc("x_nn", IntegratedBrdfNetwork, (2, 3, 3, 1), True, "FittedBrdfNNs4LayerX.hpp"),
				ModelDesc("y_nn", IntegratedBrdfNetwork, (2, 3, 3, 1), False, "FittedBrdfNNs4LayerY.hpp")
			),
		),
		(
			"3-Order expression",
			(
				ModelDesc("x_factors", IntegratedBrdfExpression, 3, True, "FittedBrdfFactorsX.hpp"),
				ModelDesc("y_factors", IntegratedBrdfExpression, 3, False, "FittedBrdfFactorsY.hpp")
			),
		),
	)
	for model in model_descs:
		print(f"Training {model[0]} ...")
		TrainModels(training_set, testing_set, args.batch_size, args.learning_rate, args.epochs, model[1])
