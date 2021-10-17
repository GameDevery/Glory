#pragma once
#include <GraphicsModule.h>
#include <GL/glew.h>
#include <ShaderData.h>
#include "GLMesh.h"

namespace Glory
{
	class OpenGLGraphicsModule : public GraphicsModule
	{
	public:
		OpenGLGraphicsModule();
		virtual ~OpenGLGraphicsModule();
		static void LogGLError(const GLenum& err, bool bIncludeTimeStamp = true);

	public: // Commands
		virtual void Clear() override;
		virtual void Swap() override;
		virtual Material* UseMaterial(MaterialData* pMaterialData) override;
		virtual void DrawMesh(MeshData* pMeshData) override;

	protected:
		virtual void OnInitialize() override;
		virtual void OnCleanup() override;
		virtual void ThreadedCleanup() override;
		virtual void ThreadedInitialize() override;
		virtual GPUResourceManager* CreateGPUResourceManager() override;
	};
}