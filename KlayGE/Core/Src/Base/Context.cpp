// Context.cpp
// KlayGE ���泡���� ʵ���ļ�
// Ver 3.11.0
// ��Ȩ����(C) ������, 2007-2010
// Homepage: http://www.klayge.org
//
// 3.11.0
// ������AudioDataSourceFactoryInstance (2010.8.22)
//
// 3.9.0
// XML��ʽ�������ļ� (2009.4.26)
//
// 3.8.0
// ������LoadCfg (2008.10.12)
//
// 3.7.0
// ���ν��� (2007.12.19)
//
// �޸ļ�¼
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KFL/CXX20/span.hpp>
#include <KFL/StringUtil.hpp>
#include <KFL/CpuInfo.hpp>
#include <KFL/DllLoader.hpp>
#include <KFL/Util.hpp>
#include <KFL/Math.hpp>
#include <KFL/Log.hpp>
#include <KlayGE/DevHelper.hpp>
#include <KlayGE/SceneManager.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/AudioFactory.hpp>
#include <KlayGE/InputFactory.hpp>
#include <KlayGE/ShowFactory.hpp>
#include <KlayGE/ScriptFactory.hpp>
#include <KlayGE/AudioDataSource.hpp>
#include <KlayGE/ResLoader.hpp>
#include <KFL/XMLDom.hpp>
#include <KlayGE/DeferredRenderingLayer.hpp>
#include <KlayGE/PerfProfiler.hpp>
#include <KlayGE/UI.hpp>
#include <KFL/Hash.hpp>

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include <boost/assert.hpp>

#if defined(KLAYGE_PLATFORM_WINDOWS)
#include <windows.h>
#if defined(KLAYGE_PLATFORM_WINDOWS_DESKTOP)
#if (_WIN32_WINNT >= _WIN32_WINNT_WINBLUE)
#include <VersionHelpers.h>
#endif
#endif
#elif defined(KLAYGE_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#endif

#include <KlayGE/Context.hpp>

#if defined(KLAYGE_PLATFORM_ANDROID) || defined(KLAYGE_PLATFORM_IOS)
	#define KLAYGE_STATIC_LINK_PLUGINS
#endif

#ifdef KLAYGE_STATIC_LINK_PLUGINS
extern "C"
{
	void MakeRenderFactory(std::unique_ptr<KlayGE::RenderFactory>& ptr);
	void MakeAudioFactory(std::unique_ptr<KlayGE::AudioFactory>& ptr);
	void MakeInputFactory(std::unique_ptr<KlayGE::InputFactory>& ptr);
	void MakeShowFactory(std::unique_ptr<KlayGE::ShowFactory>& ptr);
	void MakeScriptFactory(std::unique_ptr<KlayGE::ScriptFactory>& ptr);
	void MakeSceneManager(std::unique_ptr<KlayGE::SceneManager>& ptr);
	void MakeAudioDataSourceFactory(std::unique_ptr<KlayGE::AudioDataSourceFactory>& ptr);
}
#else
namespace KlayGE
{
	typedef void (*MakeRenderFactoryFunc)(std::unique_ptr<RenderFactory>& ptr);
	typedef void (*MakeAudioFactoryFunc)(std::unique_ptr<AudioFactory>& ptr);
	typedef void (*MakeInputFactoryFunc)(std::unique_ptr<InputFactory>& ptr);
	typedef void (*MakeShowFactoryFunc)(std::unique_ptr<ShowFactory>& ptr);
	typedef void (*MakeScriptFactoryFunc)(std::unique_ptr<ScriptFactory>& ptr);
	typedef void (*MakeSceneManagerFunc)(std::unique_ptr<SceneManager>& ptr);
	typedef void (*MakeAudioDataSourceFactoryFunc)(std::unique_ptr<AudioDataSourceFactory>& ptr);
#if KLAYGE_IS_DEV_PLATFORM
	typedef void (*MakeDevHelperFunc)(std::unique_ptr<DevHelper>& ptr);
#endif
}

#define KLAYGE_DLL_PREFIX DLL_PREFIX KFL_STRINGIZE(KLAYGE_NAME)
#endif

namespace KlayGE
{
	class Context::Impl final
	{
		KLAYGE_NONCOPYABLE(Impl);

	public:
		Impl() = default;

		static Context& Instance()
		{
			if (!context_instance_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!context_instance_)
				{
					context_instance_ = MakeUniquePtr<Context>();
				}
			}
			return *context_instance_;
		}
		static void Destroy() noexcept
		{
			std::lock_guard<std::mutex> lock(singleton_mutex_);
			if (context_instance_)
			{
				context_instance_->pimpl_->DestroyAll();
				context_instance_.reset();
			}
		}
		void Suspend()
		{
			if (scene_mgr_)
			{
				scene_mgr_->Suspend();
			}

			if (res_loader_.Valid())
			{
				res_loader_.Suspend();
			}
			if (perf_profiler_.Valid())
			{
				perf_profiler_.Suspend();
			}
			if (ui_mgr_.Valid())
			{
				ui_mgr_.Suspend();
			}

			if (deferred_rendering_layer_)
			{
				deferred_rendering_layer_->Suspend();
			}
			if (show_factory_)
			{
				show_factory_->Suspend();
			}
			if (render_factory_)
			{
				render_factory_->Suspend();
			}
			if (audio_factory_)
			{
				audio_factory_->Suspend();
			}
			if (input_factory_)
			{
				input_factory_->Suspend();
			}
			if (script_factory_)
			{
				script_factory_->Suspend();
			}
			if (audio_data_src_factory_)
			{
				audio_data_src_factory_->Suspend();
			}
		}
		void Resume()
		{
			if (scene_mgr_)
			{
				scene_mgr_->Resume();
			}

			if (res_loader_.Valid())
			{
				res_loader_.Resume();
			}
			if (perf_profiler_.Valid())
			{
				perf_profiler_.Resume();
			}
			if (ui_mgr_.Valid())
			{
				ui_mgr_.Resume();
			}

			if (deferred_rendering_layer_)
			{
				deferred_rendering_layer_->Resume();
			}
			if (show_factory_)
			{
				show_factory_->Resume();
			}
			if (render_factory_)
			{
				render_factory_->Resume();
			}
			if (audio_factory_)
			{
				audio_factory_->Resume();
			}
			if (input_factory_)
			{
				input_factory_->Resume();
			}
			if (script_factory_)
			{
				script_factory_->Resume();
			}
			if (audio_data_src_factory_)
			{
				audio_data_src_factory_->Resume();
			}
		}

#ifdef KLAYGE_PLATFORM_ANDROID
		android_app* AppState() const
		{
			return state_;
		}
		void AppState(android_app* state)
		{
			state_ = state;
		}
#endif

		void LoadCfg(std::string const& cfg_file)
		{
#if defined(KLAYGE_PLATFORM_WINDOWS_DESKTOP)
			static char const* available_rfs_array[] = {"D3D11", "OpenGL", "OpenGLES", "D3D12"};
			static char const* available_afs_array[] = {"OpenAL", "XAudio"};
			static char const* available_adsfs_array[] = {"OggVorbis"};
			static char const* available_ifs_array[] = {"MsgInput"};
			static char const* available_sfs_array[] = {"DShow", "MFShow"};
			static char const* available_scfs_array[] = {"Python"};
#elif defined(KLAYGE_PLATFORM_WINDOWS_STORE)
			static char const* available_rfs_array[] = {"D3D11", "D3D12"};
			static char const* available_afs_array[] = {"XAudio"};
			static char const* available_adsfs_array[] = {"OggVorbis"};
			static char const* available_ifs_array[] = {"MsgInput"};
			static char const* available_sfs_array[] = {"MFShow"};
			static char const* available_scfs_array[] = {"Python"};
#elif defined(KLAYGE_PLATFORM_LINUX)
			static char const* available_rfs_array[] = {"OpenGL"};
			static char const* available_afs_array[] = {"OpenAL"};
			static char const* available_adsfs_array[] = {"OggVorbis"};
			static char const* available_ifs_array[] = {"NullInput"};
			static char const* available_sfs_array[] = {"NullShow"};
			static char const* available_scfs_array[] = {"Python"};
#elif defined(KLAYGE_PLATFORM_ANDROID)
			static char const* available_rfs_array[] = {"OpenGLES"};
			static char const* available_afs_array[] = {"OpenAL"};
			static char const* available_adsfs_array[] = {"OggVorbis"};
			static char const* available_ifs_array[] = {"MsgInput"};
			static char const* available_sfs_array[] = {"NullShow"};
			static char const* available_scfs_array[] = {"NullScript"};
#elif defined(KLAYGE_PLATFORM_IOS)
			static char const* available_rfs_array[] = {"OpenGLES"};
			static char const* available_afs_array[] = {"OpenAL"};
			static char const* available_adsfs_array[] = {"OggVorbis"};
			static char const* available_ifs_array[] = {"MsgInput"};
			static char const* available_sfs_array[] = {"NullShow"};
			static char const* available_scfs_array[] = {"NullScript"};
#elif defined(KLAYGE_PLATFORM_DARWIN)
			static char const* available_rfs_array[] = {"OpenGL"};
			static char const* available_afs_array[] = {"OpenAL"};
			static char const* available_adsfs_array[] = {"OggVorbis"};
			static char const* available_ifs_array[] = {"MsgInput"};
			static char const* available_sfs_array[] = {"NullShow"};
			static char const* available_scfs_array[] = {"Python"};
#endif
			static char const* available_sms_array[] = {"OCTree"};

			uint32_t width = 800;
			uint32_t height = 600;
			ElementFormat color_fmt = EF_ARGB8;
			ElementFormat depth_stencil_fmt = EF_D16;
			uint32_t sample_count = 1;
			uint32_t sample_quality = 0;
			bool full_screen = false;
			uint32_t sync_interval = 0;
			bool hdr = false;
			bool ppaa = false;
			bool gamma = false;
			bool color_grading = false;
			float bloom = 0.25f;
			bool blue_shift = true;
			bool keep_screen_on = true;
			StereoMethod stereo_method = STM_None;
			float stereo_separation = 0;
			DisplayOutputMethod display_output_method = DOM_sRGB;
			uint32_t paper_white = 100;
			uint32_t display_max_luminance = 100;
			std::vector<std::pair<std::string, std::string>> graphics_options;
			bool debug_context = false;
			bool perf_profiler = false;
			bool location_sensor = false;

			std::string rf_name;
			std::string af_name;
			std::string if_name;
			std::string sf_name;
			std::string scf_name;
			std::string sm_name;
			std::string adsf_name;

			auto& res_loader = ResLoaderInstance();
			ResIdentifierPtr file = res_loader.Open(cfg_file);
			if (file)
			{
				XMLNode cfg_root = LoadXml(*file);

				XMLNode const* context_node = cfg_root.FirstNode("context");
				XMLNode const* graphics_node = cfg_root.FirstNode("graphics");

				if (XMLNode const* rf_node = context_node->FirstNode("render_factory"))
				{
					rf_name = std::string(rf_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* af_node = context_node->FirstNode("audio_factory"))
				{
					af_name = std::string(af_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* if_node = context_node->FirstNode("input_factory"))
				{
					if_name = std::string(if_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* sf_node = context_node->FirstNode("show_factory"))
				{
					sf_name = std::string(sf_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* scf_node = context_node->FirstNode("script_factory"))
				{
					scf_name = std::string(scf_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* sm_node = context_node->FirstNode("scene_manager"))
				{
					sm_name = std::string(sm_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* adsf_node = context_node->FirstNode("audio_data_source_factory"))
				{
					adsf_name = std::string(adsf_node->Attrib("name")->ValueString());
				}
				if (XMLNode const* perf_profiler_node = context_node->FirstNode("perf_profiler"))
				{
					perf_profiler = perf_profiler_node->Attrib("enabled")->ValueInt() ? true : false;
				}
				if (XMLNode const* location_sensor_node = context_node->FirstNode("location_sensor"))
				{
					location_sensor = location_sensor_node->Attrib("enabled")->ValueInt() ? true : false;
				}

				XMLNode const* frame_node = graphics_node->FirstNode("frame");
				if (XMLAttribute const* attr = frame_node->Attrib("width"))
				{
					width = attr->ValueUInt();
				}
				if (XMLAttribute const* attr = frame_node->Attrib("height"))
				{
					height = attr->ValueUInt();
				}
				std::string color_fmt_str = "ARGB8";
				if (XMLAttribute const* attr = frame_node->Attrib("color_fmt"))
				{
					color_fmt_str = std::string(attr->ValueString());
				}
				std::string depth_stencil_fmt_str = "D16";
				if (XMLAttribute const* attr = frame_node->Attrib("depth_stencil_fmt"))
				{
					depth_stencil_fmt_str = std::string(attr->ValueString());
				}
				if (XMLAttribute const* attr = frame_node->Attrib("fullscreen"))
				{
					full_screen = attr->ValueBool();
				}
				if (XMLAttribute const* attr = frame_node->Attrib("keep_screen_on"))
				{
					keep_screen_on = attr->ValueBool();
				}

				size_t const color_fmt_str_hash = RtHash(color_fmt_str.c_str());
				if (CtHash("ARGB8") == color_fmt_str_hash)
				{
					color_fmt = EF_ARGB8;
				}
				else if (CtHash("ABGR8") == color_fmt_str_hash)
				{
					color_fmt = EF_ABGR8;
				}
				else if (CtHash("A2BGR10") == color_fmt_str_hash)
				{
					color_fmt = EF_A2BGR10;
				}
				else if (CtHash("ABGR16F") == color_fmt_str_hash)
				{
					color_fmt = EF_ABGR16F;
				}

				size_t const depth_stencil_fmt_str_hash = RtHash(depth_stencil_fmt_str.c_str());
				if (CtHash("D16") == depth_stencil_fmt_str_hash)
				{
					depth_stencil_fmt = EF_D16;
				}
				else if (CtHash("D24S8") == depth_stencil_fmt_str_hash)
				{
					depth_stencil_fmt = EF_D24S8;
				}
				else if (CtHash("D32F") == depth_stencil_fmt_str_hash)
				{
					depth_stencil_fmt = EF_D32F;
				}
				else
				{
					depth_stencil_fmt = EF_Unknown;
				}

				XMLNode const* sample_node = frame_node->FirstNode("sample");
				if (XMLAttribute const* attr = sample_node->Attrib("count"))
				{
					sample_count = attr->ValueUInt();
				}
				if (XMLAttribute const* attr = sample_node->Attrib("quality"))
				{
					sample_quality = attr->ValueUInt();
				}

				XMLNode const* sync_interval_node = graphics_node->FirstNode("sync_interval");
				if (XMLAttribute const* attr = sync_interval_node->Attrib("value"))
				{
					sync_interval = attr->ValueUInt();
				}

				XMLNode const* hdr_node = graphics_node->FirstNode("hdr");
				if (XMLAttribute const* attr = hdr_node->Attrib("value"))
				{
					hdr = attr->ValueBool();
				}
				if (XMLAttribute const* attr = hdr_node->Attrib("bloom"))
				{
					bloom = attr->ValueFloat();
				}
				if (XMLAttribute const* attr = hdr_node->Attrib("blue_shift"))
				{
					blue_shift = attr->ValueBool();
				}

				XMLNode const* ppaa_node = graphics_node->FirstNode("ppaa");
				if (XMLAttribute const* attr = ppaa_node->Attrib("value"))
				{
					ppaa = attr->ValueBool();
				}

				XMLNode const* gamma_node = graphics_node->FirstNode("gamma");
				if (XMLAttribute const* attr = gamma_node->Attrib("value"))
				{
					gamma = attr->ValueBool();
				}

				XMLNode const* color_grading_node = graphics_node->FirstNode("color_grading");
				if (XMLAttribute const* attr = color_grading_node->Attrib("value"))
				{
					color_grading = attr->ValueBool();
				}

				XMLNode const* stereo_node = graphics_node->FirstNode("stereo");
				if (XMLAttribute const* attr = stereo_node->Attrib("method"))
				{
					size_t const method_str_hash = HashValue(attr->ValueString());
					if (CtHash("none") == method_str_hash)
					{
						stereo_method = STM_None;
					}
					else if (CtHash("red_cyan") == method_str_hash)
					{
						stereo_method = STM_ColorAnaglyph_RedCyan;
					}
					else if (CtHash("yellow_blue") == method_str_hash)
					{
						stereo_method = STM_ColorAnaglyph_YellowBlue;
					}
					else if (CtHash("green_red") == method_str_hash)
					{
						stereo_method = STM_ColorAnaglyph_GreenRed;
					}
					else if (CtHash("lcd_shutter") == method_str_hash)
					{
						stereo_method = STM_LCDShutter;
					}
					else if (CtHash("hor_interlacing") == method_str_hash)
					{
						stereo_method = STM_HorizontalInterlacing;
					}
					else if (CtHash("ver_interlacing") == method_str_hash)
					{
						stereo_method = STM_VerticalInterlacing;
					}
					else if (CtHash("horizontal") == method_str_hash)
					{
						stereo_method = STM_Horizontal;
					}
					else if (CtHash("vertical") == method_str_hash)
					{
						stereo_method = STM_Vertical;
					}
					else if (CtHash("oculus_vr") == method_str_hash)
					{
						stereo_method = STM_OculusVR;
					}
					else
					{
						stereo_method = STM_ColorAnaglyph_RedCyan;
					}
				}
				else
				{
					stereo_method = STM_None;
				}
				if (XMLAttribute const* attr = stereo_node->Attrib("separation"))
				{
					stereo_separation = attr->ValueFloat();
				}

				XMLNode const* output_node = graphics_node->FirstNode("output");
				if (XMLAttribute const* attr = output_node->Attrib("method"))
				{
					size_t const method_str_hash = HashValue(attr->ValueString());
					if (CtHash("hdr10") == method_str_hash)
					{
						display_output_method = DOM_HDR10;
					}
					else
					{
						display_output_method = DOM_sRGB;
					}
				}

				if (XMLAttribute const* attr = output_node->Attrib("white"))
				{
					paper_white = attr->ValueUInt();
				}
				if (XMLAttribute const* attr = output_node->Attrib("max_lum"))
				{
					display_max_luminance = attr->ValueUInt();
				}

				if (display_output_method == DOM_sRGB)
				{
					paper_white = display_max_luminance = 100;
				}
				else if ((color_fmt == EF_ARGB8) || (color_fmt == EF_ABGR8))
				{
					color_fmt = EF_A2BGR10;
				}

				if (XMLNode const* options_node = graphics_node->FirstNode("options"))
				{
					if (XMLAttribute const* attr = options_node->Attrib("str"))
					{
						std::string_view const options_str = attr->ValueString();

						std::vector<std::string_view> strs = StringUtil::Split(options_str, StringUtil::EqualTo(','));
						for (size_t index = 0; index < strs.size(); ++index)
						{
							std::string_view opt = StringUtil::Trim(strs[index]);
							std::string::size_type const loc = opt.find(':');
							std::string_view opt_name = opt.substr(0, loc);
							std::string_view opt_val = opt.substr(loc + 1);
							graphics_options.emplace_back(opt_name, opt_val);
						}
					}
				}

				XMLNode const* debug_context_node = graphics_node->FirstNode("debug_context");
				if (XMLAttribute const* attr = debug_context_node->Attrib("value"))
				{
					debug_context = attr->ValueBool();
				}
			}

			std::span<char const*> const available_rfs = available_rfs_array;
			std::span<char const*> const available_afs = available_afs_array;
			std::span<char const*> const available_adsfs = available_adsfs_array;
			std::span<char const*> const available_ifs = available_ifs_array;
			std::span<char const*> const available_sfs = available_sfs_array;
			std::span<char const*> const available_scfs = available_scfs_array;
			std::span<char const*> const available_sms = available_sms_array;

			if (std::find(available_rfs.begin(), available_rfs.end(), rf_name) == available_rfs.end())
			{
				rf_name = available_rfs[0];
			}
			if (std::find(available_afs.begin(), available_afs.end(), af_name) == available_afs.end())
			{
				af_name = available_afs[0];
			}
			if (std::find(available_adsfs.begin(), available_adsfs.end(), adsf_name) == available_adsfs.end())
			{
				adsf_name = available_adsfs[0];
			}
			if (std::find(available_ifs.begin(), available_ifs.end(), if_name) == available_ifs.end())
			{
				if_name = available_ifs[0];
			}
			if (std::find(available_sfs.begin(), available_sfs.end(), sf_name) == available_sfs.end())
			{
				sf_name = available_sfs[0];
			}
			if (std::find(available_scfs.begin(), available_scfs.end(), scf_name) == available_scfs.end())
			{
				scf_name = available_scfs[0];
			}
			if (std::find(available_sms.begin(), available_sms.end(), sm_name) == available_sms.end())
			{
				sm_name = available_sms[0];
			}

			cfg_.render_factory_name = std::move(rf_name);
			cfg_.audio_factory_name = std::move(af_name);
			cfg_.input_factory_name = std::move(if_name);
			cfg_.show_factory_name = std::move(sf_name);
			cfg_.script_factory_name = std::move(scf_name);
			cfg_.scene_manager_name = std::move(sm_name);
			cfg_.audio_data_source_factory_name = std::move(adsf_name);

			cfg_.graphics_cfg.left = cfg_.graphics_cfg.top = 0;
			cfg_.graphics_cfg.width = width;
			cfg_.graphics_cfg.height = height;
			cfg_.graphics_cfg.color_fmt = color_fmt;
			cfg_.graphics_cfg.depth_stencil_fmt = depth_stencil_fmt;
			cfg_.graphics_cfg.sample_count = sample_count;
			cfg_.graphics_cfg.sample_quality = sample_quality;
			cfg_.graphics_cfg.full_screen = full_screen;
			cfg_.graphics_cfg.sync_interval = sync_interval;
			cfg_.graphics_cfg.hdr = hdr;
			cfg_.graphics_cfg.ppaa = ppaa;
			cfg_.graphics_cfg.gamma = gamma;
			cfg_.graphics_cfg.color_grading = color_grading;
			cfg_.graphics_cfg.bloom = bloom;
			cfg_.graphics_cfg.blue_shift = blue_shift;
			cfg_.graphics_cfg.keep_screen_on = keep_screen_on;
			cfg_.graphics_cfg.stereo_method = stereo_method;
			cfg_.graphics_cfg.stereo_separation = stereo_separation;
			cfg_.graphics_cfg.display_output_method = display_output_method;
			cfg_.graphics_cfg.paper_white = paper_white;
			cfg_.graphics_cfg.display_max_luminance = display_max_luminance;
			cfg_.graphics_cfg.options = std::move(graphics_options);
			cfg_.graphics_cfg.debug_context = debug_context;

			cfg_.deferred_rendering = false;
			cfg_.perf_profiler = perf_profiler;
			cfg_.location_sensor = location_sensor;
		}
		void SaveCfg(std::string const& cfg_file)
		{
			XMLNode root(XMLNodeType::Element, "configure");

			{
				XMLNode context_node(XMLNodeType::Element, "context");
				{
					XMLNode rf_node(XMLNodeType::Element, "render_factory");
					rf_node.AppendAttrib(XMLAttribute("name", cfg_.render_factory_name));
					context_node.AppendNode(std::move(rf_node));

					XMLNode af_node(XMLNodeType::Element, "audio_factory");
					af_node.AppendAttrib(XMLAttribute("name", cfg_.audio_factory_name));
					context_node.AppendNode(std::move(af_node));

					XMLNode if_node(XMLNodeType::Element, "input_factory");
					if_node.AppendAttrib(XMLAttribute("name", cfg_.input_factory_name));
					context_node.AppendNode(std::move(if_node));

					XMLNode sm_node(XMLNodeType::Element, "scene_manager");
					sm_node.AppendAttrib(XMLAttribute("name", cfg_.scene_manager_name));
					context_node.AppendNode(std::move(sm_node));

					XMLNode sf_node(XMLNodeType::Element, "show_factory");
					sf_node.AppendAttrib(XMLAttribute("name", cfg_.show_factory_name));
					context_node.AppendNode(std::move(sf_node));

					XMLNode scf_node(XMLNodeType::Element, "script_factory");
					scf_node.AppendAttrib(XMLAttribute("name", cfg_.script_factory_name));
					context_node.AppendNode(std::move(scf_node));

					XMLNode adsf_node(XMLNodeType::Element, "audio_data_source_factory");
					adsf_node.AppendAttrib(XMLAttribute("name", cfg_.audio_data_source_factory_name));
					context_node.AppendNode(std::move(adsf_node));

					XMLNode perf_profiler_node(XMLNodeType::Element, "perf_profiler");
					perf_profiler_node.AppendAttrib(XMLAttribute("enabled", static_cast<uint32_t>(cfg_.perf_profiler)));
					context_node.AppendNode(std::move(perf_profiler_node));

					XMLNode location_sensor_node(XMLNodeType::Element, "location_sensor");
					location_sensor_node.AppendAttrib(XMLAttribute("enabled", static_cast<uint32_t>(cfg_.location_sensor)));
					context_node.AppendNode(std::move(location_sensor_node));
				}
				root.AppendNode(std::move(context_node));
			}
			{
				XMLNode graphics_node(XMLNodeType::Element, "graphics");
				{
					{
						XMLNode frame_node(XMLNodeType::Element, "frame");
						frame_node.AppendAttrib(XMLAttribute("width", static_cast<uint32_t>(cfg_.graphics_cfg.width)));
						frame_node.AppendAttrib(XMLAttribute("height", static_cast<uint32_t>(cfg_.graphics_cfg.height)));

						std::string color_fmt_str;
						switch (cfg_.graphics_cfg.color_fmt)
						{
						case EF_ARGB8:
							color_fmt_str = "ARGB8";
							break;

						case EF_ABGR8:
							color_fmt_str = "ABGR8";
							break;

						case EF_A2BGR10:
							color_fmt_str = "A2BGR10";
							break;

						case EF_ABGR16F:
							color_fmt_str = "ABGR16F";
							break;

						default:
							color_fmt_str = "ARGB8";
							break;
						}
						frame_node.AppendAttrib(XMLAttribute("color_fmt", color_fmt_str));

						std::string depth_stencil_fmt_str;
						switch (cfg_.graphics_cfg.depth_stencil_fmt)
						{
						case EF_D16:
							depth_stencil_fmt_str = "D16";
							break;

						case EF_D24S8:
							depth_stencil_fmt_str = "D24S8";
							break;

						case EF_D32F:
							depth_stencil_fmt_str = "D32F";
							break;

						default:
							depth_stencil_fmt_str = "None";
							break;
						}
						frame_node.AppendAttrib(XMLAttribute("depth_stencil_fmt", depth_stencil_fmt_str));

						frame_node.AppendAttrib(XMLAttribute("fullscreen", static_cast<uint32_t>(cfg_.graphics_cfg.full_screen)));
						frame_node.AppendAttrib(XMLAttribute("keep_screen_on", static_cast<uint32_t>(cfg_.graphics_cfg.keep_screen_on)));

						{
							XMLNode sample_node(XMLNodeType::Element, "sample");
							sample_node.AppendAttrib(XMLAttribute("count", cfg_.graphics_cfg.sample_count));
							sample_node.AppendAttrib(XMLAttribute("quality", cfg_.graphics_cfg.sample_quality));
							frame_node.AppendNode(std::move(sample_node));
						}

						graphics_node.AppendNode(std::move(frame_node));
					}
					{
						XMLNode sync_interval_node(XMLNodeType::Element, "sync_interval");
						sync_interval_node.AppendAttrib(XMLAttribute("value", cfg_.graphics_cfg.sync_interval));
						graphics_node.AppendNode(std::move(sync_interval_node));
					}
					{
						XMLNode hdr_node(XMLNodeType::Element, "hdr");
						hdr_node.AppendAttrib(XMLAttribute("value", cfg_.graphics_cfg.hdr));
						hdr_node.AppendAttrib(XMLAttribute("bloom", cfg_.graphics_cfg.bloom));
						hdr_node.AppendAttrib(XMLAttribute("blue_shift", static_cast<uint32_t>(cfg_.graphics_cfg.blue_shift)));
						graphics_node.AppendNode(std::move(hdr_node));
					}
					{
						XMLNode ppaa_node(XMLNodeType::Element, "ppaa");
						ppaa_node.AppendAttrib(XMLAttribute("value", static_cast<uint32_t>(cfg_.graphics_cfg.ppaa)));
						graphics_node.AppendNode(std::move(ppaa_node));
					}
					{
						XMLNode gamma_node(XMLNodeType::Element, "gamma");
						gamma_node.AppendAttrib(XMLAttribute("value", static_cast<uint32_t>(cfg_.graphics_cfg.gamma)));
						graphics_node.AppendNode(std::move(gamma_node));
					}
					{
						XMLNode color_grading_node(XMLNodeType::Element, "color_grading");
						color_grading_node.AppendAttrib(XMLAttribute("value", static_cast<uint32_t>(cfg_.graphics_cfg.color_grading)));
						graphics_node.AppendNode(std::move(color_grading_node));
					}
					{
						XMLNode stereo_node(XMLNodeType::Element, "stereo");
						std::string method_str;
						switch (cfg_.graphics_cfg.stereo_method)
						{
						case STM_None:
							method_str = "none";
							break;

						case STM_ColorAnaglyph_RedCyan:
							method_str = "red_cyan";
							break;

						case STM_ColorAnaglyph_YellowBlue:
							method_str = "yellow_blue";
							break;

						case STM_ColorAnaglyph_GreenRed:
							method_str = "green_red";
							break;

						case STM_LCDShutter:
							method_str = "lcd_shutter";
							break;

						case STM_HorizontalInterlacing:
							method_str = "hor_interlacing";
							break;

						case STM_VerticalInterlacing:
							method_str = "ver_interlacing";
							break;

						case STM_Horizontal:
							method_str = "horizontal";
							break;

						case STM_Vertical:
							method_str = "vertical";
							break;

						case STM_OculusVR:
							method_str = "oculus_vr";
							break;

						default:
							method_str = "none";
							break;
						}
						stereo_node.AppendAttrib(XMLAttribute("method", method_str));

						std::ostringstream oss;
						oss.precision(2);
						oss << std::fixed << cfg_.graphics_cfg.stereo_separation;
						stereo_node.AppendAttrib(XMLAttribute("separation", oss.str()));

						graphics_node.AppendNode(std::move(stereo_node));
					}
					{
						XMLNode output_node(XMLNodeType::Element, "output");
						std::string method_str;
						switch (cfg_.graphics_cfg.display_output_method)
						{
						case DOM_HDR10:
							method_str = "hdr10";
							break;

						default:
							method_str = "srgb";
							break;
						}
						output_node.AppendAttrib(XMLAttribute("method", method_str));

						output_node.AppendAttrib(XMLAttribute("white", cfg_.graphics_cfg.paper_white));
						output_node.AppendAttrib(XMLAttribute("max_lum", cfg_.graphics_cfg.display_max_luminance));

						graphics_node.AppendNode(std::move(output_node));
					}
					{
						XMLNode debug_context_node(XMLNodeType::Element, "debug_context");
						debug_context_node.AppendAttrib(XMLAttribute("value", static_cast<uint32_t>(cfg_.graphics_cfg.debug_context)));
						graphics_node.AppendNode(std::move(debug_context_node));
					}
				}
				root.AppendNode(std::move(graphics_node));
			}

			std::ofstream ofs(cfg_file.c_str());
			SaveXml(root, ofs);
		}

		void Config(ContextCfg const& cfg)
		{
			cfg_ = cfg;

			if (this->RenderFactoryValid())
			{
				if (cfg_.deferred_rendering)
				{
					if (!deferred_rendering_layer_)
					{
						deferred_rendering_layer_ = MakeUniquePtr<DeferredRenderingLayer>();
					}
				}
				else
				{
					deferred_rendering_layer_.reset();
				}
			}
		}
		ContextCfg const& Config() const
		{
			return cfg_;
		}

		void LoadRenderFactory(std::string const& rf_name)
		{
			render_factory_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			render_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string render_path = res_loader.Locate("Render");
			std::string fn = KLAYGE_DLL_PREFIX"_RenderEngine_" + rf_name + DLL_SUFFIX;

			std::string path = render_path + "/" + fn;
			render_loader_.Load(res_loader.Locate(path));

			auto* mrf = reinterpret_cast<MakeRenderFactoryFunc>(render_loader_.GetProcAddress("MakeRenderFactory"));
			if (mrf != nullptr)
			{
				mrf(render_factory_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				render_loader_.Free();
			}
#else
			KFL_UNUSED(rf_name);
			MakeRenderFactory(render_factory_);
#endif
		}
		void LoadAudioFactory(std::string const& af_name)
		{
			audio_factory_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			audio_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string audio_path = res_loader.Locate("Audio");
			std::string fn = KLAYGE_DLL_PREFIX"_AudioEngine_" + af_name + DLL_SUFFIX;

			std::string path = audio_path + "/" + fn;
			audio_loader_.Load(res_loader.Locate(path));

			auto* maf = reinterpret_cast<MakeAudioFactoryFunc>(audio_loader_.GetProcAddress("MakeAudioFactory"));
			if (maf != nullptr)
			{
				maf(audio_factory_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				audio_loader_.Free();
			}
#else
			KFL_UNUSED(af_name);
			MakeAudioFactory(audio_factory_);
#endif
		}
		void LoadInputFactory(std::string const& if_name)
		{
			input_factory_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			input_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string input_path = res_loader.Locate("Input");
			std::string fn = KLAYGE_DLL_PREFIX"_InputEngine_" + if_name + DLL_SUFFIX;

			std::string path = input_path + "/" + fn;
			input_loader_.Load(res_loader.Locate(path));

			auto* mif = reinterpret_cast<MakeInputFactoryFunc>(input_loader_.GetProcAddress("MakeInputFactory"));
			if (mif != nullptr)
			{
				mif(input_factory_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				input_loader_.Free();
			}
#else
			KFL_UNUSED(if_name);
			MakeInputFactory(input_factory_);
#endif
		}
		void LoadShowFactory(std::string const& sf_name)
		{
			show_factory_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			show_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string show_path = res_loader.Locate("Show");
			std::string fn = KLAYGE_DLL_PREFIX"_ShowEngine_" + sf_name + DLL_SUFFIX;

			std::string path = show_path + "/" + fn;
			show_loader_.Load(res_loader.Locate(path));

			auto* msf = reinterpret_cast<MakeShowFactoryFunc>(show_loader_.GetProcAddress("MakeShowFactory"));
			if (msf != nullptr)
			{
				msf(show_factory_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				show_loader_.Free();
			}
#else
			KFL_UNUSED(sf_name);
			MakeShowFactory(show_factory_);
#endif
		}
		void LoadScriptFactory(std::string const& sf_name)
		{
			script_factory_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			script_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string script_path = res_loader.Locate("Script");
			std::string fn = KLAYGE_DLL_PREFIX"_ScriptEngine_" + sf_name + DLL_SUFFIX;

			std::string path = script_path + "/" + fn;
			script_loader_.Load(res_loader.Locate(path));

			auto* msf = reinterpret_cast<MakeScriptFactoryFunc>(script_loader_.GetProcAddress("MakeScriptFactory"));
			if (msf != nullptr)
			{
				msf(script_factory_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				script_loader_.Free();
			}
#else
			KFL_UNUSED(sf_name);
			MakeScriptFactory(script_factory_);
#endif
		}
		void LoadSceneManager(std::string const& sm_name)
		{
			scene_mgr_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			sm_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string sm_path = res_loader.Locate("Scene");
			std::string fn = KLAYGE_DLL_PREFIX"_Scene_" + sm_name + DLL_SUFFIX;

			std::string path = sm_path + "/" + fn;
			sm_loader_.Load(res_loader.Locate(path));

			auto* msm = reinterpret_cast<MakeSceneManagerFunc>(sm_loader_.GetProcAddress("MakeSceneManager"));
			if (msm != nullptr)
			{
				msm(scene_mgr_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				sm_loader_.Free();
			}
#else
			KFL_UNUSED(sm_name);
			MakeSceneManager(scene_mgr_);
#endif
		}
		void LoadAudioDataSourceFactory(std::string const& adsf_name)
		{
			audio_data_src_factory_.reset();

#ifndef KLAYGE_STATIC_LINK_PLUGINS
			ads_loader_.Free();

			auto& res_loader = ResLoaderInstance();
			std::string adsf_path = res_loader.Locate("Audio");
			std::string fn = KLAYGE_DLL_PREFIX"_AudioDataSource_" + adsf_name + DLL_SUFFIX;

			std::string path = adsf_path + "/" + fn;
			ads_loader_.Load(res_loader.Locate(path));

			auto* madsf = reinterpret_cast<MakeAudioDataSourceFactoryFunc>(ads_loader_.GetProcAddress("MakeAudioDataSourceFactory"));
			if (madsf != nullptr)
			{
				madsf(audio_data_src_factory_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				ads_loader_.Free();
			}
#else
			KFL_UNUSED(adsf_name);
			MakeAudioDataSourceFactory(audio_data_src_factory_);
#endif
		}

#if KLAYGE_IS_DEV_PLATFORM
		void LoadDevHelper()
		{
			dev_helper_.reset();

			dev_helper_loader_.Free();

			std::string path = KLAYGE_DLL_PREFIX"_DevHelper" DLL_SUFFIX;

			auto& res_loader = ResLoaderInstance();
			dev_helper_loader_.Load(res_loader.Locate(path));

			auto* mdh = reinterpret_cast<MakeDevHelperFunc>(dev_helper_loader_.GetProcAddress("MakeDevHelper"));
			if (mdh != nullptr)
			{
				mdh(dev_helper_);
			}
			else
			{
				LogError() << "Loading " << path << " failed" << std::endl;
				dev_helper_loader_.Free();
			}
		}
#endif

		void AppInstance(App3DFramework& app) noexcept
		{
			app_ = &app;
		}
		bool AppValid() const noexcept
		{
			return app_ != nullptr;
		}
		App3DFramework& AppInstance() noexcept
		{
			BOOST_ASSERT(app_);
			KLAYGE_ASSUME(app_);
			return *app_;
		}

		bool SceneManagerValid() const noexcept
		{
			return scene_mgr_ != nullptr;
		}
		SceneManager& SceneManagerInstance()
		{
			if (!scene_mgr_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!scene_mgr_)
				{
					this->LoadSceneManager(cfg_.scene_manager_name);
				}
			}
			return *scene_mgr_;
		}

		bool RenderFactoryValid() const noexcept
		{
			return render_factory_ != nullptr;
		}
		RenderFactory& RenderFactoryInstance()
		{
			if (!render_factory_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!render_factory_)
				{
					this->LoadRenderFactory(cfg_.render_factory_name);
				}
			}
			return *render_factory_;
		}

		bool AudioFactoryValid() const noexcept
		{
			return audio_factory_ != nullptr;
		}
		AudioFactory& AudioFactoryInstance()
		{
			if (!audio_factory_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!audio_factory_)
				{
					this->LoadAudioFactory(cfg_.audio_factory_name);
				}
			}
			return *audio_factory_;
		}

		bool InputFactoryValid() const noexcept
		{
			return input_factory_ != nullptr;
		}
		InputFactory& InputFactoryInstance()
		{
			if (!input_factory_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!input_factory_)
				{
					this->LoadInputFactory(cfg_.input_factory_name);
				}
			}
			return *input_factory_;
		}

		bool ShowFactoryValid() const noexcept
		{
			return show_factory_ != nullptr;
		}
		ShowFactory& ShowFactoryInstance()
		{
			if (!show_factory_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!show_factory_)
				{
					this->LoadShowFactory(cfg_.show_factory_name);
				}
			}
			return *show_factory_;
		}

		bool ScriptFactoryValid() const noexcept
		{
			return script_factory_ != nullptr;
		}
		ScriptFactory& ScriptFactoryInstance()
		{
			if (!script_factory_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!script_factory_)
				{
					this->LoadScriptFactory(cfg_.script_factory_name);
				}
			}
			return *script_factory_;
		}

		bool AudioDataSourceFactoryValid() const noexcept
		{
			return audio_data_src_factory_ != nullptr;
		}
		AudioDataSourceFactory& AudioDataSourceFactoryInstance()
		{
			if (!audio_data_src_factory_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!audio_data_src_factory_)
				{
					this->LoadAudioDataSourceFactory(cfg_.audio_data_source_factory_name);
				}
			}
			return *audio_data_src_factory_;
		}

		bool DeferredRenderingLayerValid() const noexcept
		{
			return deferred_rendering_layer_ != nullptr;
		}
		DeferredRenderingLayer& DeferredRenderingLayerInstance()
		{
			return *deferred_rendering_layer_;
		}

#if KLAYGE_IS_DEV_PLATFORM
		bool DevHelperValid() const noexcept
		{
			return dev_helper_ != nullptr;
		}
		DevHelper& DevHelperInstance()
		{
			if (!dev_helper_)
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!dev_helper_)
				{
					this->LoadDevHelper();
				}
			}
			return *dev_helper_;
		}
#endif

		ThreadPool& ThreadPoolInstance()
		{
			return global_thread_pool_;
		}

		ResLoader& ResLoaderInstance()
		{
			if (!res_loader_.Valid())
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!res_loader_.Valid())
				{
					res_loader_.Init(global_thread_pool_);
				}
			}

			return res_loader_;
		}

		PerfProfiler& PerfProfilerInstance()
		{
			if (!perf_profiler_.Valid())
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!perf_profiler_.Valid())
				{
					perf_profiler_.Init();
				}
			}

			return perf_profiler_;
		}

		UIManager& UIManagerInstance()
		{
			if (!ui_mgr_.Valid())
			{
				std::lock_guard<std::mutex> lock(singleton_mutex_);
				if (!ui_mgr_.Valid())
				{
					ui_mgr_.Init();
				}
			}

			return ui_mgr_;
		}

	private:
		void DestroyAll()
		{
			// It must called before destructor to prevent an order issue

			scene_mgr_.reset();

			if (res_loader_.Valid())
			{
				res_loader_.Destroy();
			}
			if (perf_profiler_.Valid())
			{
				perf_profiler_.Destroy();
			}
			if (ui_mgr_.Valid())
			{
				ui_mgr_.Destroy();
			}

			deferred_rendering_layer_.reset();
			show_factory_.reset();
			render_factory_.reset();
			audio_factory_.reset();
			input_factory_.reset();
			script_factory_.reset();
			audio_data_src_factory_.reset();

#if KLAYGE_IS_DEV_PLATFORM
			dev_helper_.reset();
#endif

			app_ = nullptr;
		}

	private:
		static std::mutex singleton_mutex_;
		static std::unique_ptr<Context> context_instance_;

		ContextCfg cfg_;

#ifdef KLAYGE_PLATFORM_ANDROID
		android_app* state_;
#endif

		App3DFramework* app_ = nullptr;

		std::unique_ptr<SceneManager> scene_mgr_;

		std::unique_ptr<RenderFactory> render_factory_;
		std::unique_ptr<AudioFactory> audio_factory_;
		std::unique_ptr<InputFactory> input_factory_;
		std::unique_ptr<ShowFactory> show_factory_;
		std::unique_ptr<ScriptFactory> script_factory_;
		std::unique_ptr<AudioDataSourceFactory> audio_data_src_factory_;
		std::unique_ptr<DeferredRenderingLayer> deferred_rendering_layer_;

#if KLAYGE_IS_DEV_PLATFORM
		std::unique_ptr<DevHelper> dev_helper_;
#endif

		DllLoader render_loader_;
		DllLoader audio_loader_;
		DllLoader input_loader_;
		DllLoader show_loader_;
		DllLoader script_loader_;
		DllLoader sm_loader_;
		DllLoader ads_loader_;

#if KLAYGE_IS_DEV_PLATFORM
		DllLoader dev_helper_loader_;
#endif

		ResLoader res_loader_;
		PerfProfiler perf_profiler_;
		UIManager ui_mgr_;

		ThreadPool global_thread_pool_;
	};

	std::mutex Context::Impl::singleton_mutex_;
	std::unique_ptr<Context> Context::Impl::context_instance_;

	Context::Context()
	{
#ifdef KLAYGE_COMPILER_MSVC
#ifdef KLAYGE_DEBUG
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#endif

		pimpl_ = MakeUniquePtr<Impl>();
	}

	Context::~Context() noexcept = default;

	Context& Context::Instance()
	{
		return Impl::Instance();
	}

	void Context::Destroy() noexcept
	{
		Impl::Destroy();
	}

	void Context::Suspend()
	{
		pimpl_->Suspend();
	}

	void Context::Resume()
	{
		pimpl_->Resume();
	}

#ifdef KLAYGE_PLATFORM_ANDROID
	android_app* Context::AppState() const
	{
		return pimpl_->AppState();
	}

	void Context::AppState(android_app* state)
	{
		pimpl_->AppState(state);
	}
#endif

	void Context::LoadCfg(std::string const & cfg_file)
	{
		pimpl_->LoadCfg(cfg_file);
	}

	void Context::SaveCfg(std::string const & cfg_file)
	{
		pimpl_->SaveCfg(cfg_file);
	}

	void Context::Config(ContextCfg const & cfg)
	{
		pimpl_->Config(cfg);
	}

	ContextCfg const & Context::Config() const
	{
		return pimpl_->Config();
	}

	void Context::LoadRenderFactory(std::string const & rf_name)
	{
		pimpl_->LoadRenderFactory(rf_name);
	}

	void Context::LoadAudioFactory(std::string const & af_name)
	{
		pimpl_->LoadAudioFactory(af_name);
	}

	void Context::LoadInputFactory(std::string const & if_name)
	{
		pimpl_->LoadInputFactory(if_name);
	}

	void Context::LoadShowFactory(std::string const & sf_name)
	{
		pimpl_->LoadShowFactory(sf_name);
	}

	void Context::LoadScriptFactory(std::string const & sf_name)
	{
		pimpl_->LoadScriptFactory(sf_name);
	}

	void Context::LoadSceneManager(std::string const & sm_name)
	{
		pimpl_->LoadSceneManager(sm_name);
	}

	void Context::LoadAudioDataSourceFactory(std::string const & adsf_name)
	{
		pimpl_->LoadAudioDataSourceFactory(adsf_name);
	}

#if KLAYGE_IS_DEV_PLATFORM
	void Context::LoadDevHelper()
	{
		pimpl_->LoadDevHelper();
	}
#endif

	void Context::AppInstance(App3DFramework& app) noexcept
	{
		pimpl_->AppInstance(app);
	}

	bool Context::AppValid() const noexcept
	{
		return pimpl_->AppValid();
	}

	App3DFramework& Context::AppInstance() noexcept
	{
		return pimpl_->AppInstance();
	}

	bool Context::SceneManagerValid() const noexcept
	{
		return pimpl_->SceneManagerValid();
	}

	SceneManager& Context::SceneManagerInstance()
	{
		return pimpl_->SceneManagerInstance();
	}

	bool Context::RenderFactoryValid() const noexcept
	{
		return pimpl_->RenderFactoryValid();
	}

	RenderFactory& Context::RenderFactoryInstance()
	{
		return pimpl_->RenderFactoryInstance();
	}

	bool Context::AudioFactoryValid() const noexcept
	{
		return pimpl_->AudioFactoryValid();
	}

	AudioFactory& Context::AudioFactoryInstance()
	{
		return pimpl_->AudioFactoryInstance();
	}

	bool Context::InputFactoryValid() const noexcept
	{
		return pimpl_->InputFactoryValid();
	}

	InputFactory& Context::InputFactoryInstance()
	{
		return pimpl_->InputFactoryInstance();
	}

	bool Context::ShowFactoryValid() const noexcept
	{
		return pimpl_->ShowFactoryValid();
	}

	ShowFactory& Context::ShowFactoryInstance()
	{
		return pimpl_->ShowFactoryInstance();
	}

	bool Context::ScriptFactoryValid() const noexcept
	{
		return pimpl_->ScriptFactoryValid();
	}

	ScriptFactory& Context::ScriptFactoryInstance()
	{
		return pimpl_->ScriptFactoryInstance();
	}

	bool Context::AudioDataSourceFactoryValid() const noexcept
	{
		return pimpl_->AudioDataSourceFactoryValid();
	}

	AudioDataSourceFactory& Context::AudioDataSourceFactoryInstance()
	{
		return pimpl_->AudioDataSourceFactoryInstance();
	}

	bool Context::DeferredRenderingLayerValid() const noexcept
	{
		return pimpl_->DeferredRenderingLayerValid();
	}

	DeferredRenderingLayer& Context::DeferredRenderingLayerInstance()
	{
		return pimpl_->DeferredRenderingLayerInstance();
	}

#if KLAYGE_IS_DEV_PLATFORM
	bool Context::DevHelperValid() const noexcept
	{
		return pimpl_->DevHelperValid();
	}

	DevHelper& Context::DevHelperInstance()
	{
		return pimpl_->DevHelperInstance();
	}
#endif

	ThreadPool& Context::ThreadPoolInstance()
	{
		return pimpl_->ThreadPoolInstance();
	}

	ResLoader& Context::ResLoaderInstance()
	{
		return pimpl_->ResLoaderInstance();
	}

	PerfProfiler& Context::PerfProfilerInstance()
	{
		return pimpl_->PerfProfilerInstance();
	}

	UIManager& Context::UIManagerInstance()
	{
		return pimpl_->UIManagerInstance();
	}
}
