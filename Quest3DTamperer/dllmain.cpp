// dllmain.cpp : Definiert den Einstiegspunkt für die DLL-Anwendung.
#include "pch.h"
#include "kiero/kiero.h"
#include <d3d9.h>
#include <d3d9types.h>
#include <assert.h>
#include <detours.h>
#include <fstream>
#include <iostream>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/imfilebrowser.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>

//The Quest3D SDK isn't included in this repo!
//You'll have to get it yourself.
//Sorry.
#include <A3d_List.h>
#include <A3d_Channels.h>
#include <A3d_ChannelGroup.h>
#include <A3d_EngineInterface.h>
#include <Aco_String.h>
#include <Aco_Float.h>
#include <Aco_DX8_D3DDeviceUse.h>
#include <Aco_DX8_ObjectData.h>
#include <Aco_DX8_Texture.h>

//UGraphviz
#include "UGraphviz/UGraphviz.hpp"

//Graphviz
// #include <graphviz/gvc.h>

#include "ExternalFunc.h"

using namespace Ubpa;

//DISCLAIMER: I have literally never done DirectX stuff before
typedef long(__stdcall* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
static Reset oReset = NULL;
typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
static EndScene oEndScene = NULL;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

uintptr_t FindFunction(const char* module, const char* symbol);

namespace ExtFunc {
#ifdef _WIN64
#include "quest3d_api.x64.inl"
#else
#include "quest3d_api.win32.inl"
#endif
}

static bool init = false;
bool showMenu;
HWND gameHandle;
WNDPROC g_WndProc_o;
ImGui::FileBrowser saveGroupFileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir);
ImGui::FileBrowser saveTextureFileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir);
ImGui::FileBrowser saveGraphFileDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir);
ImGui::FileBrowser loadGroupFileDialog(0);
ImGui::FileBrowser loadTextureFileDialog(0);
EngineInterface* engine = nullptr;
int channelGroupToUse = 0;
int channelInGroupToUse = 1;
int mipmapLevelToUse = 0;
char newGroupName[128] = "New Group";
char newText[1024] = "New Text";
bool textureLocked = false;
bool previewTexture = true;
float newFloat = 0;
char newScript[20000] = "-- Script here!";
UGraphviz::Graph* channelGraph;

int channelDumpIndex = 0;
int childrenDumpIndex = 0;

//credits to Zerkg for finding out how to use an A3d_String
struct Offset
{
    char buffer[4];
    const char* str;
};

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(const std::string& str)
{
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void ToClipboard(HWND hwnd, const std::string& s) {
    OpenClipboard(hwnd);
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (!hg) {
        CloseClipboard();
        return;
    }
    memcpy(GlobalLock(hg), s.c_str(), s.size() + 1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT, hg);
    CloseClipboard();
    GlobalFree(hg);
}

std::size_t replace(std::string& inout, std::string_view what, std::string_view with)
{
    std::size_t count{};
    for (std::string::size_type pos{};
        inout.npos != (pos = inout.find(what.data(), pos, what.length()));
        pos += with.length(), ++count) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
}

static void __fastcall CallChannelHook(A3d_Channel* self, DWORD edx)
{ 
    ExtFunc::TrueCallChannel(self);
    if (engine == nullptr) {
        engine = self->engine;
    }
}

std::string GetChannelValue(A3d_Channel* channel)
{
    OLECHAR* guidOLECHAR;
    StringFromCLSID(channel->GetChannelType().guid, &guidOLECHAR);
    std::wstring wstring = std::wstring(guidOLECHAR);
    std::string guidstr = utf8_encode(wstring);
    auto guid(channel->GetChannelType().guid);


    if (guid == STRING_GUID)
    {
        auto channelString = ExtFunc::StringChannel_GetString((Aco_StringChannel*)channel);
        if (channelString)
        {
            return channelString;
        }
        return "(nullptr)";
    }
    if (guid == FLOAT_CHANNEL_GUID) {
        std::string resultString;
        resultString += std::to_string(ExtFunc::Aco_FloatChannel_GetFloat(channel));
        resultString += "\nDefault Value: ";
        resultString += std::to_string(ExtFunc::Aco_FloatChannel_GetDefaultFloat(channel));
        return resultString;
    }
    if (strstr(guidstr.c_str(), "F26BB40B-B196")) {
        auto channelString = ExtFunc::StringOperator_GetString(channel);
        if (channelString)
        {
            return channelString;
        }
        return "(nullptr)";
    }

    return "";
}

//i swear i was just high
//might redo at some point but it does function
void writeChannel(A3d_ChannelGroup* group, UGraphviz::Graph* graph)
{
	auto& registry = graph->GetRegistry();

	for (int i{}; i < 50000; ++i)
	{
		A3d_Channel* channel = ExtFunc::ChannelGroup_GetChannel(group, i);
		if (channel)
		{
			GUID channelGuid(channel->GetChannelType().guid);
			auto node = registry.RegisterNode(std::to_string(ExtFunc::Channel_GetChannelIDIndexNr(channel)));

			std::string nodeLabel;
			nodeLabel += ExtFunc::Channel_GetChannelName(channel);
			nodeLabel += "\\n";
			nodeLabel += channel->GetChannelType().name;
			std::string channelValue = GetChannelValue(channel);
            //Backslashes need to be escaped
            replace(channelValue, "\\", "\\\\");
            //Graphviz can't cope with newlines either, so they need to be replaced with the literal "\n"
            replace(channelValue, "\r\n", "\\n");
            replace(channelValue, "\n", "\\n");
            //Escape quotes as well!
            replace(channelValue, "\"", "\\\"");
			if (!channelValue.empty())
			{
				nodeLabel += "\\nChannel value: ";
				nodeLabel += channelValue;
			}
			registry.RegisterNodeAttr(node, UGraphviz::Attrs_label, nodeLabel);
			registry.RegisterNodeAttr(node, UGraphviz::Attrs_shape, "box");
			if (ExtFunc::Channel_GetChannelIDIndexNr(channel) == 0)
			{
				registry.RegisterNodeAttr(node, UGraphviz::Attrs_color, "green");
			}
			graph->AddNode(node);
		}
	}

	for (int channelnum{}; channelnum < 50000; ++channelnum)
	{
		A3d_Channel* channel = ExtFunc::ChannelGroup_GetChannel(group, channelnum);

		if (channel)
		{
			const int children = ExtFunc::Channel_GetChildCount(channel);

			for (int childnum{}; childnum < children; ++childnum)
			{
				A3d_Channel* child(ExtFunc::Channel_GetChild(channel, childnum));

				if (child)
				{
					if (registry.IsRegisteredNode(std::to_string(ExtFunc::Channel_GetChannelIDIndexNr(child))) && registry.IsRegisteredNode(std::to_string(ExtFunc::Channel_GetChannelIDIndexNr(channel))))
					{
						auto edge = registry.RegisterEdge(registry.GetNodeIndex(std::to_string(ExtFunc::Channel_GetChannelIDIndexNr(channel))), registry.GetNodeIndex(std::to_string(ExtFunc::Channel_GetChannelIDIndexNr(child))));
						graph->AddEdge(edge);
					}
				}
			}
		}
	}
}

LRESULT __stdcall CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) && showMenu)
        return true;

    return CallWindowProc(g_WndProc_o, hWnd, uMsg, wParam, lParam);
}


long __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    long result = oReset(pDevice, pPresentationParameters);
    ImGui_ImplDX9_CreateDeviceObjects();

    return result;
}

long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (!init)
    {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(params.hFocusWindow);
        ImGui_ImplDX9_Init(pDevice);

        saveGroupFileDialog.SetTitle("Save channel group");
        saveGroupFileDialog.SetTypeFilters({ ".cgr" });

        loadGroupFileDialog.SetTitle("Load channel group");
        loadGroupFileDialog.SetTypeFilters({ ".cgr" });

        saveTextureFileDialog.SetTitle("Save texture");
        saveTextureFileDialog.SetTypeFilters({ ".tga", ".png", ".jpg"});

        loadTextureFileDialog.SetTitle("Load texture");
        loadTextureFileDialog.SetTypeFilters({ ".tga", ".png", ".jpg" });

        saveGraphFileDialog.SetTitle("Save DOT Digraph");
        saveGraphFileDialog.SetTypeFilters({ ".gv" });

        init = true;
    }

    if (showMenu) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        //Because Audiosurf has its own cursor and it'll just be below the ImGUI window
        //It also ignores all other windows above it and registers clicks through them. No workaround for this (yet), this is Quest3D/Audiosurf's fault.
        ImGui::GetIO().MouseDrawCursor = true;

        A3d_ChannelGroup* group = nullptr;
        A3d_Channel* channel = nullptr;
        std::string channelGUID;
        
        ImGui::Begin("Quest3DTamperer");
        ImGui::Spacing();
        if (engine != nullptr) {
            if (ImGui::CollapsingHeader("Channel groups")) {
                //ImGui::Text("There is %i channel groups.", engine->GetChannelGroupCount());
                ImGui::Spacing();

                ImGui::InputText("Pool name for new group", newGroupName, IM_ARRAYSIZE(newGroupName));
                if (ImGui::Button("Load channel group file")) {
                    loadGroupFileDialog.Open();
                }
                ImGui::Spacing();

                ImGui::InputInt("A3d_ChannelGroup to use", &channelGroupToUse, 1, 10);
                ImGui::Spacing();

                // group = engine->GetChannelGroup(channelGroupToUse);
                group = ExtFunc::EngineInterface_GetChannelGroup(engine, channelGroupToUse);
                if (group != nullptr) {
                    ImGui::Text("Info of current channel group:");
                    ImGui::Text(ExtFunc::ChannelGroup_GetPoolName(group));
                    ImGui::Text(ExtFunc::ChannelGroup_GetChannelGroupFileName(group));
                    ImGui::Text("Is group protected: %s", ExtFunc::ChannelGroup_GetGroupIsProtected(group) ? "true" : "false");
                    ImGui::Text("Is group read-only: %s", ExtFunc::ChannelGroup_GetGroupIsProtected(group) ? "true" : "false");
                    if (ImGui::Button("Save group without protection")) {
                        ExtFunc::ChannelGroup_SetReadOnly(group, false); //cope
                        ExtFunc::ChannelGroup_SetGroupIsProtected(group, false); //seethe
                        saveGroupFileDialog.Open(); // mald
                    }
                    ImGui::Spacing();

                    ImGui::Text("Group has %i channels", ExtFunc::ChannelGroup_GetChannelCount(group));
                    ImGui::InputInt("Channel in group to get", &channelInGroupToUse, 1, 10);
                    channel = ExtFunc::ChannelGroup_GetChannel(group, channelInGroupToUse);
                    if (channel != nullptr) {
                        ImGui::Text(ExtFunc::Channel_GetChannelName(channel));
                        
                        ImGui::Text(channel->GetChannelType().name);
                        
                        OLECHAR* guidString;
                        StringFromCLSID(channel->GetChannelType().guid, &guidString);
                        std::wstring wstring = std::wstring(guidString);
                        std::string stdstringGUID = utf8_encode(wstring);
                        channelGUID = stdstringGUID;

                        ImGui::Text(stdstringGUID.c_str());

                        if (ImGui::Button("Save as DOT Digraph")) {
                            saveGraphFileDialog.Open();
                        }

                        if (strstr(stdstringGUID.c_str(), "6E6FB247-4627")) {
                            Aco_StringChannel* stringChannel(reinterpret_cast<Aco_StringChannel*>(channel));

                        	ImGui::Text("Text in channel: %s", ExtFunc::StringChannel_GetString(stringChannel));
                            
                            if (ImGui::Button("Copy to clipboard")) {
                                ToClipboard(gameHandle, ExtFunc::StringChannel_GetString((Aco_StringChannel*)channel));
                            }
                            ImGui::Spacing();

                            ImGui::InputText("New text to set", newText, IM_ARRAYSIZE(newText));
                            if (ImGui::Button("Set text")) {
                                ExtFunc::StringChannel_SetString((Aco_StringChannel*)channel, newText);
                            }
                        }

                        if (strstr(stdstringGUID.c_str(), "F26BB40B-B196")) {
                            ImGui::Text("Text in channel: %s", ExtFunc::StringOperator_GetString(channel));
                        }
                        
                        if (strstr(stdstringGUID.c_str(), "6514FE12-88CF")) {
                            ImGui::Text("Script: \n%s", ExtFunc::Lua_GetScript(channel));
                            
                            if (ImGui::Button("Copy to clipboard")) {
                                ToClipboard(gameHandle, ExtFunc::Lua_GetScript(channel));
                            }
                            ImGui::Spacing();
                            
                            ImGui::InputTextMultiline("New script", newScript, IM_ARRAYSIZE(newScript));
                            if (ImGui::Button("Set script")) {
                                ExtFunc::Lua_SetScript(channel, newScript);
                            }
                        }

                        if (strstr(stdstringGUID.c_str(), "BC052C38-2D5D")) {
                            Aco_DX8_Texture* texture = (Aco_DX8_Texture*)channel;
                            ImGui::Text("Mipmap level count: %d", ExtFunc::Aco_DX8_Texture_GetMipMapLevels(texture));
                            ImGui::InputInt("Select Mipmap level", &mipmapLevelToUse, 1, 10);
                            ImGui::Checkbox("Enable preview", &previewTexture);

                            //You should turn off the preview when swapping textures
                            //Else the game WILL crash
                            if (!textureLocked && previewTexture) {
                                IDirect3DTexture9* d3dTexture = ExtFunc::Aco_DX8_Texture_GetTexture(texture);
                                D3DSURFACE_DESC description = ExtFunc::Aco_DX8_Texture_GetTextureDescription(texture, mipmapLevelToUse);
                                ImGui::Text("Texture size: %dx%d", description.Width, description.Height);
                                ImGui::Image((void*)d3dTexture, ImVec2(description.Width, description.Height));
                            }

                            if(ImGui::Button("Save texture")) {
                                saveTextureFileDialog.Open();
                            }
                            if (ImGui::Button("Load texture")) {
                                loadTextureFileDialog.Open();
                            }
                        }

                        if (strstr(stdstringGUID.c_str(), "376A9C13-8D66")) {
                            D3DMATERIAL9 material = ExtFunc::Aco_DX8_MaterialChannel_GetMaterial(channel);
                            ImGui::Text("Power: %f", material.Power);

                            ImVec4 specular = ImVec4(material.Specular.r, material.Specular.g, material.Specular.b, material.Specular.a);
                            ImGui::TextColored(specular, "Specular color");
                            ImVec4 emissive = ImVec4(material.Emissive.r, material.Emissive.g, material.Emissive.b, material.Emissive.a);
                            ImGui::TextColored(emissive, "Emissive color");
                            ImVec4 ambient = ImVec4(material.Ambient.r, material.Ambient.g, material.Ambient.b, material.Ambient.a);
                            ImGui::TextColored(ambient, "Ambient color");
                            ImVec4 diffuse = ImVec4(material.Diffuse.r, material.Diffuse.g, material.Diffuse.b, material.Diffuse.a);
                            ImGui::TextColored(diffuse, "Diffuse color");
                        }

                        if (strstr(stdstringGUID.c_str(), "21A8923D-B908")) {
                            Aco_DX8_ObjectDataChannel* objectData = (Aco_DX8_ObjectDataChannel*)channel;
                            ImGui::Text("Vertex count: %d", ExtFunc::Aco_DX8_ObjectDataChannel_GetVertexCount(objectData));
                        }

                        if (strstr(stdstringGUID.c_str(), "10C20C0A-7A55")) {
                            //Commenting this out for now because it just results in an access violation
                            //D3DXVECTOR3 objectPosition = Aco_DX8_ObjectChannel_GetPosition(channel);
                            
                            //ImGui::Text("Object position:\nX - %d\nY - %d\nZ - %d", objectPosition.x, objectPosition.y, objectPosition.z);
                        }

                        if (strstr(stdstringGUID.c_str(), "BE69CCC4-CFC1")) {
                            ImGui::Text("Float value: %f", ExtFunc::Aco_FloatChannel_GetFloat(channel));
                            ImGui::Text("Default value: %f", ExtFunc::Aco_FloatChannel_GetDefaultFloat(channel));
                            ImGui::InputFloat("New float", &newFloat);
                            if (ImGui::Button("Set float")) {
                                ExtFunc::Aco_FloatChannel_SetFloat(channel, newFloat);
                            }
                        }

                        if (strstr(stdstringGUID.c_str(), "9D045960-EAC2")) {
                            //D3DXVECTOR3 vector = Aco_VectorChannel_GetVector(channel);
                            //ImGui::Text("Vector value: %f, %f, %f", vector.x, vector.y, vector.z);
                        }

                        ImGui::Spacing();
                    }
                }
                else {
                    ImGui::Text("Group not found!");
                }
            }
        }
        else {
            ImGui::Text("Please perform any action in-game.\nThis is needed to get the pointer to the EngineInterface,\nwhich is neccessary for Quest3DTamperer to work.");
        }
        ImGui::End();

        saveGroupFileDialog.Display();
        loadGroupFileDialog.Display();
        saveTextureFileDialog.Display();
        loadTextureFileDialog.Display();
        saveGraphFileDialog.Display();

        if (saveGroupFileDialog.HasSelected())
        {
            ExtFunc::ChannelGroup_SaveChannelGroup(group, saveGroupFileDialog.GetSelected().string().c_str());
            saveGroupFileDialog.ClearSelected();
        }
        if (loadGroupFileDialog.HasSelected())
        {
            A3d_ChannelGroup* newGroup = engine->LoadChannelGroup(loadGroupFileDialog.GetSelected().string().c_str(), newGroupName);
            //Call the group's start channel
            if (newGroup != nullptr)
            {
                ExtFunc::ChannelGroup_CallStartChannel(newGroup);
            }
            loadGroupFileDialog.ClearSelected();
        }
        if (saveTextureFileDialog.HasSelected())
        {
            std::ofstream binaryFile(saveTextureFileDialog.GetSelected().string().c_str(), std::ios::out | std::ios::binary);
            if (binaryFile.is_open())
            {
                Aco_DX8_Texture* texture = (Aco_DX8_Texture*)channel;
                char* data = ExtFunc::Aco_DX8_Texture_GetTextureBuffer(texture);
                int size = ExtFunc::Aco_DX8_Texture_GetBufferSize(texture);
                binaryFile.write(data, size);
            }
            loadGroupFileDialog.ClearSelected();
        }
        if (loadTextureFileDialog.HasSelected())
        {
            if (strstr(channelGUID.c_str(), "BC052C38-2D5D")) {
                textureLocked = true;
                Aco_DX8_Texture* texture = (Aco_DX8_Texture*)channel;
                D3DSURFACE_DESC description = ExtFunc::Aco_DX8_Texture_GetTextureDescription(texture, mipmapLevelToUse);
                D3DLOCKED_RECT lockedRect;
                lockedRect.pBits = (void*)ExtFunc::Aco_DX8_Texture_GetTextureBuffer(texture);
                lockedRect.Pitch = description.Width * 4;

                ExtFunc::Aco_DX8_Texture_LockTexture(texture, mipmapLevelToUse, lockedRect);
                ExtFunc::Aco_DX8_Texture_LoadTextureFromFile(texture, (char*)loadTextureFileDialog.GetSelected().string().c_str());
                ExtFunc::Aco_DX8_Texture_UnlockTexture(texture, mipmapLevelToUse);
                textureLocked = false;
            }
            loadGroupFileDialog.ClearSelected();
        }
        if (saveGraphFileDialog.HasSelected()) {
            channelDumpIndex = 0;
            childrenDumpIndex = 0;
            channelGraph = new UGraphviz::Graph(ExtFunc::ChannelGroup_GetPoolName(group), true);
            writeChannel(group, channelGraph);

            std::ofstream file(saveGraphFileDialog.GetSelected().string().c_str(), std::ofstream::trunc);
			std::string dotSource = channelGraph->Dump();
            file << dotSource.c_str();
            file.close();

            saveGraphFileDialog.ClearSelected();
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    return oEndScene(pDevice);
}

struct handle_data {
    unsigned long process_id;
    HWND window_handle;
};

BOOL is_main_window(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
    handle_data& data = *(handle_data*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !is_main_window(handle))
        return TRUE;
    data.window_handle = handle;
    return FALSE;
}

HWND find_main_window(unsigned long process_id)
{
    handle_data data {
      .process_id = process_id,
      .window_handle = nullptr
    };

    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}

void d3d9_init()
{
    if (kiero::bind(16, (void**)&oReset, hkReset) != kiero::Status::Success) {
        spdlog::error("Could not bind to D3D9 Reset");
        return;
    }

    if (kiero::bind(42, (void**)&oEndScene, hkEndScene) != kiero::Status::Success) {
        spdlog::error("Could not bind to D3D9 EndScene");
        return;
    }

    gameHandle = find_main_window(GetCurrentProcessId());

#ifdef _WIN64
    g_WndProc_o = (WNDPROC)SetWindowLongPtr(gameHandle, GWLP_WNDPROC, (LRESULT)hkWndProc);
#else
    g_WndProc_o = (WNDPROC)SetWindowLong(gameHandle, GWL_WNDPROC, (LRESULT)hkWndProc);
#endif
}

int kieroExampleThread()
{
    if (kiero::init(kiero::RenderType::Auto) == kiero::Status::Success)
    {
        if (kiero::getRenderType() == kiero::RenderType::D3D9)
        {
            d3d9_init();
        }

        while (true)
        {
            //toggle menu

            // try load engine ptr from global variable
            if (!engine) {
#ifdef _WIN64
                uintptr_t globalEnginePtr = FindFunction("highpoly.dll", "?globalEngine@@3PEAVEngineInterface@@EA");
#else
                uintptr_t globalEnginePtr = FindFunction("highpoly.dll", "?globalEngine@@3PAVEngineInterface@@A");
#endif
                if (globalEnginePtr) {
                    EngineInterface* globalEngine = *reinterpret_cast<EngineInterface**>(globalEnginePtr);
                  if (globalEngine) {
                      engine = globalEngine;
                  }
                }
            }

            if (GetAsyncKeyState(VK_END) & 1)
            {
                if (showMenu) {
                    showMenu = false;
                }
                else {
                    showMenu = true;
                }
            }

            //force re-init of ImGUI
            if (GetAsyncKeyState(VK_INSERT) & 1)
            {
                init = false;
            }
        }

        return 1;
    }

    return 0;
}

uintptr_t FindFunction(const char* module, const char* symbol) {
  auto func = reinterpret_cast<uintptr_t>(DetourFindFunction(module, symbol));
  if (!func) {
      spdlog::error("Could not find symbol '{}' in [{}]", symbol, module);
  }
  return func;
}

void FindExternalFunctions() {
#ifdef _WIN64
    ExtFunc::EngineInterface_GetChannelGroup =
        FindFunction("highpoly.dll", "?GetChannelGroup@EngineInterface@@UEAAPEAVA3d_ChannelGroup@@H@Z");

    ExtFunc::TrueCallChannel = FindFunction("highpoly.dll", "?CallChannel@A3d_Channel@@UEAAXXZ");

    ExtFunc::ChannelGroup_GetChannelGroupFileName =
        FindFunction("highpoly.dll", "?GetChannelGroupFileName@A3d_ChannelGroup@@UEAAPEBDXZ");
    ExtFunc::ChannelGroup_GetPoolName =
        FindFunction("highpoly.dll", "?GetPoolName@A3d_ChannelGroup@@UEAAPEBDXZ");
    ExtFunc::ChannelGroup_GetChannelCount =
        FindFunction("highpoly.dll", "?GetChannelCount@A3d_ChannelGroup@@UEAAHXZ");
    ExtFunc::ChannelGroup_GetGroupIsProtected =
        FindFunction("highpoly.dll", "?GetGroupIsProtected@A3d_ChannelGroup@@UEAA_NXZ");
    ExtFunc::ChannelGroup_GetReadOnly =
        FindFunction("highpoly.dll", "?GetReadOnly@A3d_ChannelGroup@@UEAA_NXZ");
    ExtFunc::ChannelGroup_SetGroupIsProtected =
        FindFunction("highpoly.dll", "?SetGroupIsProtected@A3d_ChannelGroup@@UEAAX_N@Z");
    ExtFunc::ChannelGroup_SetReadOnly =
        FindFunction("highpoly.dll", "?SetReadOnly@A3d_ChannelGroup@@UEAAX_N@Z");
    ExtFunc::ChannelGroup_SaveChannelGroup =
        FindFunction("highpoly.dll", "?SaveChannelGroup@A3d_ChannelGroup@@UEAA_NPEBD@Z");
    ExtFunc::ChannelGroup_GetGroupIndex =
        FindFunction("highpoly.dll", "?GetGroupIndex@A3d_ChannelGroup@@UEAAHXZ");
    ExtFunc::ChannelGroup_CallStartChannel =
        FindFunction("highpoly.dll", "?CallStartChannel@A3d_ChannelGroup@@UEAAXXZ");
    ExtFunc::ChannelGroup_GetChannel =
        FindFunction("highpoly.dll", "?GetChannel@A3d_ChannelGroup@@UEAAPEAVA3d_Channel@@H@Z");

    ExtFunc::Channel_GetChannelName =
        FindFunction("highpoly.dll", "?GetChannelName@A3d_Channel@@QEAAPEBDXZ");
    ExtFunc::Channel_GetChild =
        FindFunction("highpoly.dll", "?GetChild@A3d_Channel@@QEAAPEAV1@H@Z");
    ExtFunc::Channel_GetChildCount =
        FindFunction("highpoly.dll", "?GetChildCount@A3d_Channel@@QEAAHXZ");
    ExtFunc::Channel_GetChannelIDIndexNr =
        FindFunction("highpoly.dll", "?GetChannelIDIndexNr@A3d_Channel@@QEAAHXZ");

    ExtFunc::StringChannel_GetString =
        FindFunction("6E6FB247-4627-4FBE-8973-48344F23881E.dll", "?GetString@Aco_StringChannel@@UEAAPEBDXZ");
    ExtFunc::StringOperator_GetString =
        FindFunction("F26BB40B-B196-4AB9-B59E-FA7C8FF436F9.dll", "?GetString@Aco_StringOperator@@UEAAPEBDXZ");
    ExtFunc::Lua_GetScript =
        FindFunction("6514FE12-88CF-480B-A3D8-7730C0CD23B3.dll", "?GetScript@Aco_Lua@@UEAAPEBDXZ");

    ExtFunc::StringChannel_SetString =
        FindFunction("6E6FB247-4627-4FBE-8973-48344F23881E.dll", "?SetString@Aco_StringChannel@@UEAAXPEBD@Z");
    ExtFunc::Lua_SetScript =
        FindFunction("6514FE12-88CF-480B-A3D8-7730C0CD23B3.dll", "?SetScript@Aco_Lua@@UEAA_NPEBD@Z");

    ExtFunc::Aco_DX8_Texture_GetDesiredWidth =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetDesiredWidth@Aco_DX8_Texture@@UEAAHXZ");
    ExtFunc::Aco_DX8_Texture_GetDesiredHeight =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetDesiredHeight@Aco_DX8_Texture@@UEAAHXZ");
    ExtFunc::Aco_DX8_Texture_GetTexture =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetTexture@Aco_DX8_Texture@@UEAAPEAUIDirect3DTexture9@@XZ");
    ExtFunc::Aco_DX8_Texture_GetTextureBuffer =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetTextureBuffer@Aco_DX8_Texture@@UEAAPEADXZ");
    ExtFunc::Aco_DX8_Texture_GetBufferSize =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetBufferSize@Aco_DX8_Texture@@UEAAHXZ");
    ExtFunc::Aco_DX8_Texture_LoadTextureFromFile =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?LoadTextureFromFile@Aco_DX8_Texture@@UEAA_NPEAD@Z");
    ExtFunc::Aco_DX8_Texture_LockTexture =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?LockTexture@Aco_DX8_Texture@@UEAAJHAEAU_D3DLOCKED_RECT@@@Z");
    ExtFunc::Aco_DX8_Texture_UnlockTexture =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?UnlockTexture@Aco_DX8_Texture@@UEAAXH@Z");
    ExtFunc::Aco_DX8_Texture_GetMipMapLevels =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetMipMapLevels@Aco_DX8_Texture@@UEAAHXZ");
    ExtFunc::Aco_DX8_Texture_GetTextureDescription =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetTextureDescription@Aco_DX8_Texture@@UEAA?AU_D3DSURFACE_DESC@@H@Z");

    ExtFunc::Aco_DX8_MaterialChannel_GetMaterial =
        FindFunction("376A9C13-8D66-49EC-BAE5-D59BE13BC519.dll", "?GetMaterialValue@Aco_DX8_MaterialChannel@@UEAAMH@Z");

    ExtFunc::Aco_DX8_ObjectDataChannel_GetVertexCount =
        FindFunction("21A8923D-B908-4104-AE88-B6718D8A8678.dll", "?GetVertexCount@Aco_DX8_ObjectDataChannel@@UEAAHXZ");
    ExtFunc::Aco_DX8_ObjectDataChannel_GetVertexPosition =
        FindFunction("21A8923D-B908-4104-AE88-B6718D8A8678.dll", "?GetVertexPosition@Aco_DX8_ObjectDataChannel@@UEAA?AUD3DXVECTOR3@@K@Z");

    ExtFunc::Aco_DX8_ObjectChannel_GetPosition =
        FindFunction("10C20C0A-7A55-4084-8676-95E5699BCEC2.dll", "?GetPosition@Aco_DX8_ObjectChannel@@UEAA?AUD3DXVECTOR3@@XZ");

    ExtFunc::Aco_FloatChannel_GetFloat =
        FindFunction("BE69CCC4-CFC1-4362-AC81-767D199BBFC3.dll", "?GetFloat@Aco_FloatChannel@@UEAAMXZ");
    ExtFunc::Aco_FloatChannel_GetDefaultFloat =
        FindFunction("BE69CCC4-CFC1-4362-AC81-767D199BBFC3.dll", "?GetDefaultFloat@Aco_FloatChannel@@UEAAMXZ");
    ExtFunc::Aco_FloatChannel_SetFloat =
        FindFunction("BE69CCC4-CFC1-4362-AC81-767D199BBFC3.dll", "?SetFloat@Aco_FloatChannel@@UEAAXM@Z");

    ExtFunc::Aco_VectorChannel_GetVector =
        FindFunction("9D045960-EAC2-4C40-9BBF-10F32F7FA305.dll", "?GetVector@Aco_VectorChannel@@UEAA?AUD3DXVECTOR3@@XZ");
#else
    ExtFunc::EngineInterface_GetChannelGroup =
        FindFunction("highpoly.dll", "?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z");

    ExtFunc::TrueCallChannel = FindFunction("highpoly.dll", "?CallChannel@A3d_Channel@@UAEXXZ");

    ExtFunc::ChannelGroup_GetChannelGroupFileName =
        FindFunction("highpoly.dll", "?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ");
    ExtFunc::ChannelGroup_GetPoolName =
        FindFunction("highpoly.dll", "?GetPoolName@A3d_ChannelGroup@@UAEPBDXZ");
    ExtFunc::ChannelGroup_GetChannelCount =
        FindFunction("highpoly.dll", "?GetChannelCount@A3d_ChannelGroup@@UAEHXZ");
    ExtFunc::ChannelGroup_GetGroupIsProtected =
        FindFunction("highpoly.dll", "?GetGroupIsProtected@A3d_ChannelGroup@@UAE_NXZ");
    ExtFunc::ChannelGroup_GetReadOnly =
        FindFunction("highpoly.dll", "?GetReadOnly@A3d_ChannelGroup@@UAE_NXZ");
    ExtFunc::ChannelGroup_SetGroupIsProtected =
        FindFunction("highpoly.dll", "?SetGroupIsProtected@A3d_ChannelGroup@@UAEX_N@Z");
    ExtFunc::ChannelGroup_SetReadOnly =
        FindFunction("highpoly.dll", "?SetReadOnly@A3d_ChannelGroup@@UAEX_N@Z");
    ExtFunc::ChannelGroup_SaveChannelGroup =
        FindFunction("highpoly.dll", "?SaveChannelGroup@A3d_ChannelGroup@@UAE_NPBD@Z");
    ExtFunc::ChannelGroup_GetGroupIndex =
        FindFunction("highpoly.dll", "?GetGroupIndex@A3d_ChannelGroup@@UAEHXZ");
    ExtFunc::ChannelGroup_CallStartChannel =
        FindFunction("highpoly.dll", "?CallStartChannel@A3d_ChannelGroup@@UAEXXZ");
    ExtFunc::ChannelGroup_GetChannel =
        FindFunction("highpoly.dll", "?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z");

    ExtFunc::Channel_GetChannelName =
        FindFunction("highpoly.dll", "?GetChannelName@A3d_Channel@@QAEPBDXZ");
    ExtFunc::Channel_GetChild =
        FindFunction("highpoly.dll", "?GetChild@A3d_Channel@@QAEPAV1@H@Z");
    ExtFunc::Channel_GetChildCount =
        FindFunction("highpoly.dll", "?GetChildCount@A3d_Channel@@QAEHXZ");
    ExtFunc::Channel_GetChannelIDIndexNr =
        FindFunction("highpoly.dll", "?GetChannelIDIndexNr@A3d_Channel@@QAEHXZ");

    ExtFunc::StringChannel_GetString =
        FindFunction("6E6FB247-4627-4FBE-8973-48344F23881E.dll", "?GetString@Aco_StringChannel@@UAEPBDXZ");
    ExtFunc::StringOperator_GetString =
        FindFunction("F26BB40B-B196-4AB9-B59E-FA7C8FF436F9.dll", "?GetString@Aco_StringOperator@@UAEPBDXZ");
    ExtFunc::Lua_GetScript =
        FindFunction("6514FE12-88CF-480B-A3D8-7730C0CD23B3.dll", "?GetScript@Aco_Lua@@UAEPBDXZ");

    ExtFunc::StringChannel_SetString =
        FindFunction("6E6FB247-4627-4FBE-8973-48344F23881E.dll", "?SetString@Aco_StringChannel@@UAEXPBD@Z");
    ExtFunc::Lua_SetScript =
        FindFunction("6514FE12-88CF-480B-A3D8-7730C0CD23B3.dll", "?SetScript@Aco_Lua@@UAE_NPBD@Z");

    ExtFunc::Aco_DX8_Texture_GetDesiredWidth =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetDesiredWidth@Aco_DX8_Texture@@UAEHXZ");
    ExtFunc::Aco_DX8_Texture_GetDesiredHeight =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetDesiredHeight@Aco_DX8_Texture@@UAEHXZ");
    ExtFunc::Aco_DX8_Texture_GetTexture =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetTexture@Aco_DX8_Texture@@UAEPAUIDirect3DTexture9@@XZ");
    ExtFunc::Aco_DX8_Texture_GetTextureBuffer =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetTextureBuffer@Aco_DX8_Texture@@UAEPADXZ");
    ExtFunc::Aco_DX8_Texture_GetBufferSize =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetBufferSize@Aco_DX8_Texture@@UAEHXZ");
    ExtFunc::Aco_DX8_Texture_LoadTextureFromFile =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?LoadTextureFromFile@Aco_DX8_Texture@@UAE_NPAD@Z");
    ExtFunc::Aco_DX8_Texture_LockTexture =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?LockTexture@Aco_DX8_Texture@@UAEJHAAU_D3DLOCKED_RECT@@@Z");
    ExtFunc::Aco_DX8_Texture_UnlockTexture =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?UnlockTexture@Aco_DX8_Texture@@UAEXH@Z");
    ExtFunc::Aco_DX8_Texture_GetMipMapLevels =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetMipMapLevels@Aco_DX8_Texture@@UAEHXZ");
    ExtFunc::Aco_DX8_Texture_GetTextureDescription =
        FindFunction("BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll", "?GetTextureDescription@Aco_DX8_Texture@@UAE?AU_D3DSURFACE_DESC@@H@Z");

    ExtFunc::Aco_DX8_MaterialChannel_GetMaterial =
        FindFunction("376A9C13-8D66-49EC-BAE5-D59BE13BC519.dll", "?GetMaterialValue@Aco_DX8_MaterialChannel@@UAEMH@Z");

    ExtFunc::Aco_DX8_ObjectDataChannel_GetVertexCount =
        FindFunction("21A8923D-B908-4104-AE88-B6718D8A8678.dll", "?GetVertexCount@Aco_DX8_ObjectDataChannel@@UAEHXZ");
    ExtFunc::Aco_DX8_ObjectDataChannel_GetVertexPosition =
        FindFunction("21A8923D-B908-4104-AE88-B6718D8A8678.dll", "?GetVertexPosition@Aco_DX8_ObjectDataChannel@@UAE?AUD3DXVECTOR3@@K@Z");

    ExtFunc::Aco_DX8_ObjectChannel_GetPosition =
        FindFunction("10C20C0A-7A55-4084-8676-95E5699BCEC2.dll", "?GetPosition@Aco_DX8_ObjectChannel@@UAE?AUD3DXVECTOR3@@XZ");

    ExtFunc::Aco_FloatChannel_GetFloat =
        FindFunction("BE69CCC4-CFC1-4362-AC81-767D199BBFC3.dll", "?GetFloat@Aco_FloatChannel@@UAEMXZ");
    ExtFunc::Aco_FloatChannel_GetDefaultFloat =
        FindFunction("BE69CCC4-CFC1-4362-AC81-767D199BBFC3.dll", "?GetDefaultFloat@Aco_FloatChannel@@UAEMXZ");
    ExtFunc::Aco_FloatChannel_SetFloat =
        FindFunction("BE69CCC4-CFC1-4362-AC81-767D199BBFC3.dll", "?SetFloat@Aco_FloatChannel@@UAEXM@Z");

    ExtFunc::Aco_VectorChannel_GetVector =
        FindFunction("9D045960-EAC2-4C40-9BBF-10F32F7FA305.dll", "?GetVector@Aco_VectorChannel@@UAE?AUD3DXVECTOR3@@XZ");
#endif
}


void* pPtr = nullptr;


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    DisableThreadLibraryCalls(hModule);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        // set up logger
        auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("msvc_logger", sink);
        spdlog::set_default_logger(logger);

        spdlog::debug("DLL Startup");

        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)kieroExampleThread, NULL, 0, NULL);

        DetourRestoreAfterWith();

        FindExternalFunctions();

        pPtr = reinterpret_cast<PVOID>(uintptr_t(ExtFunc::TrueCallChannel));
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&pPtr, CallChannelHook);
        DetourTransactionCommit();
        // reset TrueCallChannel location to detour'd code cave
        ExtFunc::TrueCallChannel = reinterpret_cast<uintptr_t>(pPtr);

        break;
    }

    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
