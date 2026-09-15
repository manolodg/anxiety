#pragma once

// Render Hardware Interface (RHI) — cabecera paraguas ----------------------------------------------
// Incluye esta única cabecera para importar todos los tipos e interfaces de la RHI. Aquí no se
// expone ningún tipo específico de un backend concreto (D3D12/Vulkan/Metal).
#include "RHITypes.h"
#include "IDevice.h"
#include "ISwapchain.h"
#include "ICommandBuffer.h"
#include "IBuffer.h"
#include "ITexture.h"
#include "IQueue.h"
