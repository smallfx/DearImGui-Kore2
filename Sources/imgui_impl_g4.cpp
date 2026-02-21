// dear imgui: Renderer for G4

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp
// https://github.com/ocornut/imgui

#include <kore3/system.h>
#include <kore3/image.h>

#include <kong.h>

#include "imgui.h"
#include "imgui_impl_g4.h"

#include <stdio.h>

// G4 data
/*static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGIFactory*            g_pFactory = NULL;
static ID3D11Buffer*            g_pVB = NULL;
static ID3D11Buffer*            g_pIB = NULL;
static ID3D10Blob*              g_pVertexShaderBlob = NULL;
static ID3D11VertexShader*      g_pVertexShader = NULL;
static ID3D11InputLayout*       g_pInputLayout = NULL;
static ID3D11Buffer*            g_pVertexConstantBuffer = NULL;
static ID3D10Blob*              g_pPixelShaderBlob = NULL;
static ID3D11PixelShader*       g_pPixelShader = NULL;
static ID3D11SamplerState*      g_pFontSampler = NULL;
static ID3D11ShaderResourceView*g_pFontTextureView = NULL;
static ID3D11RasterizerState*   g_pRasterizerState = NULL;
static ID3D11BlendState*        g_pBlendState = NULL;
static ID3D11DepthStencilState* g_pDepthStencilState = NULL;*/
static kore_gpu_device *g_KoreDevice;
static kore_gpu_command_list *g_KoreCommandList;
static kore_gpu_texture g_FontTexture;
static kore_gpu_buffer g_FontImageBuffer;
static kore_gpu_sampler g_FontSampler;
static bool g_FontSamplerInitialized = false;
static kore_gpu_buffer g_ImguiShaderConstants;
static imgui_shader_uniforms_set g_ImguiShaderUniforms;
static kore_gpu_buffer g_IB;
static bool g_IndexBufferInitialized = false;
static vertex_in_buffer g_VB;
static bool g_VertexBufferInitialized = false;
static int g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;
static bool first_update = true;
static uint32_t font_image_width = 0;
static uint32_t font_image_height = 0;
static int tex_stride = 0;

static void ImGui_ImplG4_SetupRenderState(ImDrawData *draw_data) {
  // Setup viewport
  kore_gpu_command_list_set_viewport(g_KoreCommandList, 0, 0, (int)draw_data->DisplaySize.x, (int)draw_data->DisplaySize.y, 0.0, 1.0);

  // Setup shader and vertex buffers
  unsigned int stride = sizeof(ImDrawVert);
  unsigned int offset = 0;
  /*ctx->IASetInputLayout(g_pInputLayout);
  ctx->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);
  ctx->IASetIndexBuffer(g_pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->VSSetShader(g_pVertexShader, NULL, 0);
  ctx->VSSetConstantBuffers(0, 1, &g_pVertexConstantBuffer);
  ctx->PSSetShader(g_pPixelShader, NULL, 0);
  ctx->PSSetSamplers(0, 1, &g_pFontSampler);
  ctx->GSSetShader(NULL, NULL, 0);
  ctx->HSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
  ctx->DSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..
  ctx->CSSetShader(NULL, NULL, 0); // In theory we should backup and restore this as well.. very infrequently used..*/

  kong_set_render_pipeline_imgui_pipeline(g_KoreCommandList);

  // Setup blend state
  /*const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
  ctx->OMSetBlendState(g_pBlendState, blend_factor, 0xffffffff);
  ctx->OMSetDepthStencilState(g_pDepthStencilState, 0);
  ctx->RSSetState(g_pRasterizerState);*/
}

// Render function
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
void ImGui_ImplG4_RenderDrawData(ImDrawData *draw_data) {
  // Avoid rendering when minimized
  if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
    return;

  // Create and grow vertex/index buffers if needed
  if (!g_VertexBufferInitialized || g_VertexBufferSize < draw_data->TotalVtxCount) {
    if (g_VertexBufferInitialized) {
      kong_destroy_buffer_vertex_in(&g_VB);
    }
    g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
    kong_create_buffer_vertex_in(g_KoreDevice, g_VertexBufferSize, &g_VB);
    g_VertexBufferInitialized = true;
  }
  if (!g_IndexBufferInitialized || g_IndexBufferSize < draw_data->TotalIdxCount) {
    if (g_IndexBufferInitialized) {
      kore_gpu_buffer_destroy(&g_IB);
    }
    g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
    kore_gpu_buffer_parameters params = {
      .size        = (uint64_t)g_IndexBufferSize * sizeof(uint32_t),
      .usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE,
    };
    kore_gpu_device_create_buffer(g_KoreDevice, &params, &g_IB);
    g_IndexBufferInitialized = true;
  }

  if (first_update) {
    kore_gpu_image_copy_buffer source = {
        .buffer         = &g_FontImageBuffer,
        .bytes_per_row  = (uint32_t)tex_stride,
        .rows_per_image = font_image_height,
    };

    kore_gpu_image_copy_texture destination = {
        .texture = &g_FontTexture,
    };

    kore_gpu_command_list_copy_buffer_to_texture(g_KoreCommandList, &source, &destination, font_image_width, font_image_height, 1);

    first_update = false;
  }


  uint32_t total_index_count = 0;

  {
    vertex_in *vtx_dst = kong_vertex_in_buffer_lock(&g_VB);
    ImDrawIdx *idx_dst = (ImDrawIdx *)kore_gpu_buffer_lock_all(&g_IB);
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
      const ImDrawList *cmd_list = draw_data->CmdLists[n];
      // memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
      for (int i = 0; i < cmd_list->VtxBuffer.Size; ++i) {
        vtx_dst[i].pos.x = cmd_list->VtxBuffer.Data[i].pos.x;
        vtx_dst[i].pos.y = cmd_list->VtxBuffer.Data[i].pos.y;
        vtx_dst[i].pos.z = 1.0;
        vtx_dst[i].uv.x = cmd_list->VtxBuffer.Data[i].uv.x;
        vtx_dst[i].uv.y = cmd_list->VtxBuffer.Data[i].uv.y;
        vtx_dst[i].col.w = ((cmd_list->VtxBuffer.Data[i].col >> 24) & 0xff) / 255.0f;
        vtx_dst[i].col.z = ((cmd_list->VtxBuffer.Data[i].col >> 16) & 0xff) / 255.0f;
        vtx_dst[i].col.y = ((cmd_list->VtxBuffer.Data[i].col >> 8) & 0xff) / 255.0f;
        vtx_dst[i].col.x = ((cmd_list->VtxBuffer.Data[i].col >> 0) & 0xff) / 255.0f;
      }
      total_index_count += cmd_list->IdxBuffer.Size;
      memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
      vtx_dst += cmd_list->VtxBuffer.Size;
      idx_dst += cmd_list->IdxBuffer.Size;
    }
    kore_gpu_buffer_unlock(&g_IB);
    kong_vertex_in_buffer_unlock(&g_VB);
  }

  kong_set_vertex_buffer_vertex_in(g_KoreCommandList, &g_VB);
  kore_gpu_command_list_set_index_buffer(g_KoreCommandList, &g_IB, KORE_GPU_INDEX_FORMAT_UINT32, 0);

  // Setup desired DX state
  ImGui_ImplG4_SetupRenderState(draw_data);

  // Setup orthographic projection matrix into our constant buffer
  // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0)
  // for single viewport apps.

  float L = draw_data->DisplayPos.x;
  float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
  float T = draw_data->DisplayPos.y;
  float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

  kore_matrix4x4 mvp;

  kore_matrix4x4_set(&mvp, 0, 0, 2.0f / (R - L));
  kore_matrix4x4_set(&mvp, 0, 1, 0.0f);
  kore_matrix4x4_set(&mvp, 0, 2, 0.0f);
  kore_matrix4x4_set(&mvp, 0, 3, 0.0f);

  kore_matrix4x4_set(&mvp, 1, 0, 0.0f);
  kore_matrix4x4_set(&mvp, 1, 1, 2.0f / (T - B));
  kore_matrix4x4_set(&mvp, 1, 2, 0.0f);
  kore_matrix4x4_set(&mvp, 1, 3, 0.0f);

  kore_matrix4x4_set(&mvp, 2, 0, 0.0f);
  kore_matrix4x4_set(&mvp, 2, 1, 0.0f);
  kore_matrix4x4_set(&mvp, 2, 2, 0.5f);
  kore_matrix4x4_set(&mvp, 2, 3, 0.0f);

  kore_matrix4x4_set(&mvp, 3, 0, (R + L) / (L - R));
  kore_matrix4x4_set(&mvp, 3, 1, (T + B) / (B - T));
  kore_matrix4x4_set(&mvp, 3, 2, 0.5f);
  kore_matrix4x4_set(&mvp, 3, 3, 1.0f);


  imgui_shader_constants_type *constants_data = imgui_shader_constants_type_buffer_lock(&g_ImguiShaderConstants, 0, 1);
  constants_data->mvp            = mvp;
  imgui_shader_constants_type_buffer_unlock(&g_ImguiShaderConstants);

  kong_set_descriptor_set_imgui_shader_uniforms(g_KoreCommandList, &g_ImguiShaderUniforms);

  // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
  /*struct BACKUP_DX11_STATE
  {
      UINT                        ScissorRectsCount, ViewportsCount;
      D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
      D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
      ID3D11RasterizerState*      RS;
      ID3D11BlendState*           BlendState;
      FLOAT                       BlendFactor[4];
      UINT                        SampleMask;
      UINT                        StencilRef;
      ID3D11DepthStencilState*    DepthStencilState;
      ID3D11ShaderResourceView*   PSShaderResource;
      ID3D11SamplerState*         PSSampler;
      ID3D11PixelShader*          PS;
      ID3D11VertexShader*         VS;
      ID3D11GeometryShader*       GS;
      UINT                        PSInstancesCount, VSInstancesCount, GSInstancesCount;
      ID3D11ClassInstance         *PSInstances[256], *VSInstances[256], *GSInstances[256];   // 256 is max according to PSSetShader documentation
      D3D11_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
      ID3D11Buffer*               IndexBuffer, *VertexBuffer, *VSConstantBuffer;
      UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
      DXGI_FORMAT                 IndexBufferFormat;
      ID3D11InputLayout*          InputLayout;
  };
  BACKUP_DX11_STATE old;
  old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  ctx->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
  ctx->RSGetViewports(&old.ViewportsCount, old.Viewports);
  ctx->RSGetState(&old.RS);
  ctx->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);
  ctx->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
  ctx->PSGetShaderResources(0, 1, &old.PSShaderResource);
  ctx->PSGetSamplers(0, 1, &old.PSSampler);
  old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
  ctx->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
  ctx->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
  ctx->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
  ctx->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

  ctx->IAGetPrimitiveTopology(&old.PrimitiveTopology);
  ctx->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
  ctx->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
  ctx->IAGetInputLayout(&old.InputLayout);*/

  // Render command lists
  // (Because we merged all buffers into a single one, we maintain our own offset into them)
  int global_idx_offset = 0;
  int global_vtx_offset = 0;
  ImVec2 clip_off = draw_data->DisplayPos;
  ImVec2 clip_scale = draw_data->FramebufferScale;
  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback != NULL) {
        // User callback, registered via ImDrawList::AddCallback()
        // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
        if (pcmd->UserCallback == ImDrawCallback_ResetRenderState) {
          ImGui_ImplG4_SetupRenderState(draw_data);
        } else {
          pcmd->UserCallback(cmd_list, pcmd);
        }
      }
      else {
        // Project scissor/clipping rectangles into framebuffer space
        ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
        ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
        if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
          continue;

        // Apply scissor/clipping rectangle
        kore_gpu_command_list_set_scissor_rect(g_KoreCommandList, (int)clip_min.x, (int)clip_min.y, (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));

        // Draw
        kore_gpu_command_list_draw_indexed(g_KoreCommandList, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
      }
    }
    global_idx_offset += cmd_list->IdxBuffer.Size;
    global_vtx_offset += cmd_list->VtxBuffer.Size;
  }

  // Restore modified DX state
  /*ctx->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
  ctx->RSSetViewports(old.ViewportsCount, old.Viewports);
  ctx->RSSetState(old.RS); if (old.RS) old.RS->Release();
  ctx->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask); if (old.BlendState) old.BlendState->Release();
  ctx->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef); if (old.DepthStencilState) old.DepthStencilState->Release();
  ctx->PSSetShaderResources(0, 1, &old.PSShaderResource); if (old.PSShaderResource) old.PSShaderResource->Release();
  ctx->PSSetSamplers(0, 1, &old.PSSampler); if (old.PSSampler) old.PSSampler->Release();
  ctx->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();
  for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();
  ctx->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();
  ctx->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();
  ctx->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();
  for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();
  ctx->IASetPrimitiveTopology(old.PrimitiveTopology);
  ctx->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset); if (old.IndexBuffer) old.IndexBuffer->Release();
  ctx->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.VertexBuffer) old.VertexBuffer->Release();
  ctx->IASetInputLayout(old.InputLayout); if (old.InputLayout) old.InputLayout->Release();*/
}

static void ImGui_ImplG4_CreateFontsTexture() {
  // Build texture atlas
  ImGuiIO &io = ImGui::GetIO();
  int width, height;
  int bpp;
  unsigned char *font_pixels;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &width, &height, &bpp);

  font_image_width = width;
  font_image_height = height;

  // Prepare image buffer for copy on first render
  {
    kore_gpu_buffer_parameters buffer_parameters;
    tex_stride = kore_gpu_device_align_texture_row_bytes(g_KoreDevice, width * bpp);
    buffer_parameters.size        = tex_stride * height;
    buffer_parameters.usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE | KORE_GPU_BUFFER_USAGE_COPY_SRC;
    kore_gpu_device_create_buffer(g_KoreDevice, &buffer_parameters, &g_FontImageBuffer);

    unsigned char *tex = (unsigned char *)kore_gpu_buffer_lock_all(&g_FontImageBuffer);
    for (int y = 0; y < height; ++y) {
      memcpy(&tex[y * tex_stride], &font_pixels[y * width * bpp], width * bpp);
    }
    kore_gpu_buffer_unlock(&g_FontImageBuffer);
  }

  // Create font texture objects
  {
    kore_gpu_texture_parameters texture_parameters = {
        .width                 = (uint32_t)width,
        .height                = (uint32_t)height,
        .depth_or_array_layers = 1,
        .mip_level_count       = 1,
        .sample_count          = 1,
        .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
        .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
        .usage                 = KORE_GPU_TEXTURE_USAGE_COPY_DST | pix_texture_texture_usage_flags(),
    };
    kore_gpu_device_create_texture(g_KoreDevice, &texture_parameters, &g_FontTexture);

    kore_gpu_sampler_parameters sampler_parameters = {
      .address_mode_u = KORE_GPU_ADDRESS_MODE_REPEAT,
      .address_mode_v = KORE_GPU_ADDRESS_MODE_REPEAT,
      .address_mode_w = KORE_GPU_ADDRESS_MODE_REPEAT,
      .mag_filter     = KORE_GPU_FILTER_MODE_LINEAR,
      .min_filter     = KORE_GPU_FILTER_MODE_LINEAR,
      .mipmap_filter  = KORE_GPU_MIPMAP_FILTER_MODE_NEAREST,
      .lod_min_clamp  = 1,
      .lod_max_clamp  = 32,
      .compare        = KORE_GPU_COMPARE_FUNCTION_UNDEFINED,
      .max_anisotropy = 1,
    };
    kore_gpu_device_create_sampler(g_KoreDevice, &sampler_parameters, &g_FontSampler);
  }

  // Store our identifier
  io.Fonts->TexID = (ImTextureID)&g_FontTexture;

  g_FontSamplerInitialized = true;
}

bool ImGui_ImplG4_CreateDeviceObjects() {
  if (g_FontSamplerInitialized)
    ImGui_ImplG4_InvalidateDeviceObjects();

  // By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
  // If you would like to use this DX11 sample code but remove this dependency you can:
  //  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred
  //  solution] 2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
  // See https://github.com/ocornut/imgui/pull/638 for sources and details.


  // Create the blending setup
  /*{
      D3D11_BLEND_DESC desc;
      ZeroMemory(&desc, sizeof(desc));
      desc.AlphaToCoverageEnable = false;
      desc.RenderTarget[0].BlendEnable = true;
      desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
      desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
      desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
      desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
      desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
      desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
      desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
      g_pd3dDevice->CreateBlendState(&desc, &g_pBlendState);
  }

  // Create the rasterizer state
  {
      D3D11_RASTERIZER_DESC desc;
      ZeroMemory(&desc, sizeof(desc));
      desc.FillMode = D3D11_FILL_SOLID;
      desc.CullMode = D3D11_CULL_NONE;
      desc.ScissorEnable = true;
      desc.DepthClipEnable = true;
      g_pd3dDevice->CreateRasterizerState(&desc, &g_pRasterizerState);
  }

  // Create depth-stencil State
  {
      D3D11_DEPTH_STENCIL_DESC desc;
      ZeroMemory(&desc, sizeof(desc));
      desc.DepthEnable = false;
      desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
      desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
      desc.StencilEnable = false;
      desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
      desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
      desc.BackFace = desc.FrontFace;
      g_pd3dDevice->CreateDepthStencilState(&desc, &g_pDepthStencilState);
  }*/

  ImGui_ImplG4_CreateFontsTexture();
  
  imgui_shader_constants_type_buffer_create(g_KoreDevice, &g_ImguiShaderConstants, 1);

  {
    imgui_shader_uniforms_parameters parameters = {
        .imgui_shader_constants = &g_ImguiShaderConstants,
        .pix_texture =
            {
                .texture           = &g_FontTexture,
                .base_mip_level    = 0,
                .mip_level_count   = 1,
                .base_array_layer  = 0,
                .array_layer_count = 1,
            },
        .pix_sampler = &g_FontSampler,
    };
    kong_create_imgui_shader_uniforms_set(g_KoreDevice, &parameters, &g_ImguiShaderUniforms);
  }

  return true;
}

void ImGui_ImplG4_InvalidateDeviceObjects() {
  /*if (!g_pd3dDevice)
      return;

  if (g_pFontSampler) { g_pFontSampler->Release(); g_pFontSampler = NULL; }
  if (g_pFontTextureView) { g_pFontTextureView->Release(); g_pFontTextureView = NULL; ImGui::GetIO().Fonts->TexID = NULL; } // We copied g_pFontTextureView to
  io.Fonts->TexID so let's clear that as well. if (g_pIB) { g_pIB->Release(); g_pIB = NULL; } if (g_pVB) { g_pVB->Release(); g_pVB = NULL; }

  if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = NULL; }
  if (g_pDepthStencilState) { g_pDepthStencilState->Release(); g_pDepthStencilState = NULL; }
  if (g_pRasterizerState) { g_pRasterizerState->Release(); g_pRasterizerState = NULL; }
  if (g_pPixelShader) { g_pPixelShader->Release(); g_pPixelShader = NULL; }
  if (g_pPixelShaderBlob) { g_pPixelShaderBlob->Release(); g_pPixelShaderBlob = NULL; }
  if (g_pVertexConstantBuffer) { g_pVertexConstantBuffer->Release(); g_pVertexConstantBuffer = NULL; }
  if (g_pInputLayout) { g_pInputLayout->Release(); g_pInputLayout = NULL; }
  if (g_pVertexShader) { g_pVertexShader->Release(); g_pVertexShader = NULL; }
  if (g_pVertexShaderBlob) { g_pVertexShaderBlob->Release(); g_pVertexShaderBlob = NULL; }*/
}

bool ImGui_ImplG4_Init(kore_gpu_device *device, kore_gpu_command_list *commandlist) {
  // Setup back-end capabilities flags
  ImGuiIO &io = ImGui::GetIO();
  io.BackendRendererName = "imgui_impl_kore3";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

  g_KoreDevice = device;
  g_KoreCommandList = commandlist;

  // Get factory from device
  /*IDXGIDevice* pDXGIDevice = NULL;
  IDXGIAdapter* pDXGIAdapter = NULL;
  IDXGIFactory* pFactory = NULL;

  if (device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) == S_OK)
      if (pDXGIDevice->GetParent(IID_PPV_ARGS(&pDXGIAdapter)) == S_OK)
          if (pDXGIAdapter->GetParent(IID_PPV_ARGS(&pFactory)) == S_OK)
          {
              g_pd3dDevice = device;
              g_pd3dDeviceContext = device_context;
              g_pFactory = pFactory;
          }
  if (pDXGIDevice) pDXGIDevice->Release();
  if (pDXGIAdapter) pDXGIAdapter->Release();
  g_pd3dDevice->AddRef();
  g_pd3dDeviceContext->AddRef();*/

  return true;
}

void ImGui_ImplG4_Shutdown() {
  ImGui_ImplG4_InvalidateDeviceObjects();
  /*if (g_pFactory) { g_pFactory->Release(); g_pFactory = NULL; }
  if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
  if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }*/
}

void ImGui_ImplG4_NewFrame() {
  if (!g_FontSamplerInitialized)
    ImGui_ImplG4_CreateDeviceObjects();
}
