
ExternalFunction<void(__fastcall*)(A3d_Channel* self)> TrueCallChannel;
// Calling the functions normally results in a game crash with an error about the ESP value not being saved correctly.
// May be related to an SDK version mismatch?
// TODO: Investigate this further at some point
ExternalFunction<const char* (__fastcall*)(A3d_ChannelGroup* self)>                                   ChannelGroup_GetChannelGroupFileName;
ExternalFunction<const char* (__fastcall*)(A3d_ChannelGroup* self)>                                   ChannelGroup_GetPoolName;
ExternalFunction<int(__fastcall*)(A3d_ChannelGroup* self)>                                            ChannelGroup_GetChannelCount;
ExternalFunction<A3d_Channel* (__fastcall*)(A3d_ChannelGroup* self, int)>                             ChannelGroup_GetChannel;
ExternalFunction<bool(__fastcall*)(A3d_ChannelGroup* self)>                                           ChannelGroup_GetGroupIsProtected;
ExternalFunction<bool(__fastcall*)(A3d_ChannelGroup* self)>                                           ChannelGroup_GetReadOnly;
ExternalFunction<void(__fastcall*)(A3d_ChannelGroup* self, bool newValue)>                            ChannelGroup_SetGroupIsProtected;
ExternalFunction<void(__fastcall*)(A3d_ChannelGroup* self, bool newValue)>                            ChannelGroup_SetReadOnly;
ExternalFunction<bool(__fastcall*)(A3d_ChannelGroup* self, const char* fileName)>                     ChannelGroup_SaveChannelGroup;
ExternalFunction<int(__fastcall*)(A3d_ChannelGroup* self)>                                            ChannelGroup_GetGroupIndex;
ExternalFunction<void(__fastcall*)(A3d_ChannelGroup* self)>                                           ChannelGroup_CallStartChannel;
ExternalFunction<const char* (__fastcall*)(A3d_Channel* self)>                                        Channel_GetChannelName;
ExternalFunction<A3d_Channel* (__fastcall*)(A3d_Channel* self, int childNr)>                          Channel_GetChild;
ExternalFunction<int(__fastcall*)(A3d_Channel* self)>                                                 Channel_GetChildCount;
ExternalFunction<int(__fastcall*)(A3d_Channel* self)>                                                 Channel_GetChannelIDIndexNr;
ExternalFunction<const char* (__fastcall*)(Aco_StringChannel* self)>                                  StringChannel_GetString;
ExternalFunction<const char* (__fastcall*)(void* self)>                                               StringOperator_GetString;
ExternalFunction<const char* (__fastcall*)(void* self)>                                               Lua_GetScript;
ExternalFunction<void(__fastcall*)(Aco_StringChannel* self, const char* string)>                      StringChannel_SetString;
ExternalFunction<BOOL(__fastcall*)(void* self, const char* string)>                                   Lua_SetScript;
ExternalFunction<int(__fastcall*)(Aco_DX8_Texture* self)>                                             Aco_DX8_Texture_GetDesiredWidth;
ExternalFunction<int(__fastcall*)(Aco_DX8_Texture* self)>                                             Aco_DX8_Texture_GetDesiredHeight;
ExternalFunction<IDirect3DTexture9* (__fastcall*)(Aco_DX8_Texture* self)>                             Aco_DX8_Texture_GetTexture;
ExternalFunction<char* (__fastcall*)(Aco_DX8_Texture* self)>                                          Aco_DX8_Texture_GetTextureBuffer;
ExternalFunction<int(__fastcall*)(Aco_DX8_Texture* self)>                                             Aco_DX8_Texture_GetBufferSize;
ExternalFunction<BOOL(__fastcall*)(Aco_DX8_Texture* self, char* path)>                                Aco_DX8_Texture_LoadTextureFromFile;
ExternalFunction<HRESULT(__fastcall*)(Aco_DX8_Texture* self, int level, D3DLOCKED_RECT& pLockedRect)> Aco_DX8_Texture_LockTexture;
ExternalFunction<void(__fastcall*)(Aco_DX8_Texture* self, int level)>                                 Aco_DX8_Texture_UnlockTexture;
ExternalFunction<int(__fastcall*)(Aco_DX8_Texture* self)>                                             Aco_DX8_Texture_GetMipMapLevels;
ExternalFunction<D3DSURFACE_DESC(__fastcall*)(Aco_DX8_Texture* self, int lvl)>                        Aco_DX8_Texture_GetTextureDescription;
ExternalFunction<D3DMATERIAL9(__fastcall*)(void* self)>                                               Aco_DX8_MaterialChannel_GetMaterial;
ExternalFunction<int(__fastcall*)(Aco_DX8_ObjectDataChannel* self)>                                   Aco_DX8_ObjectDataChannel_GetVertexCount;
ExternalFunction<D3DXVECTOR3(__fastcall*)(Aco_DX8_ObjectDataChannel* self, DWORD nr)>                 Aco_DX8_ObjectDataChannel_GetVertexPosition;
ExternalFunction<D3DXVECTOR3(__fastcall*)(void* self)>                                                Aco_DX8_ObjectChannel_GetPosition;
ExternalFunction<float(__fastcall*)(void* self)>                                                      Aco_FloatChannel_GetFloat;
ExternalFunction<float(__fastcall*)(void* self)>                                                      Aco_FloatChannel_GetDefaultFloat;
ExternalFunction<void(__fastcall*)(void* self, float value)>                                          Aco_FloatChannel_SetFloat;
ExternalFunction<D3DXVECTOR3(__fastcall*)(void* self)>                                                Aco_VectorChannel_GetVector;
ExternalFunction<A3d_ChannelGroup* (__fastcall*)(EngineInterface* self, int index)>                   EngineInterface_GetChannelGroup;