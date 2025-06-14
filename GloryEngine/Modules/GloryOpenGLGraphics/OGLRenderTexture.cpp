#include "OGLRenderTexture.h"
#include "OpenGLGraphicsModule.h"
#include "Debug.h"
#include "GloryOGL.h"
#include "GLConverter.h"

#include <GL/glew.h>
#include <Engine.h>

namespace Glory
{
	OGLRenderTexture::OGLRenderTexture(const RenderTextureCreateInfo& createInfo) : RenderTexture(createInfo), m_GLFrameBufferID(0)
	{
	}

	OGLRenderTexture::~OGLRenderTexture()
	{
		if (m_GLFrameBufferID != NULL) glDeleteFramebuffers(1, &m_GLFrameBufferID);
		OpenGLGraphicsModule::LogGLError(glGetError());
		m_GLFrameBufferID = NULL;
	}

	void OGLRenderTexture::ReadColorPixel(const std::string& attachment, const glm::ivec2& coord, void* value, DataType type)
	{
		BindForDraw();
		const uint32_t index = (uint32_t)m_NameToTextureIndex.at(attachment);
		const GLuint format = GLConverter::TO_GLFORMAT.at(m_CreateInfo.Attachments[index].Format);
		const GLenum dataType = GLConverter::TO_GLDATATYPE.at(type);
		glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
		OpenGLGraphicsModule::LogGLError(glGetError());
		glReadPixels(coord.x, coord.y, 1, 1, format, dataType, value);
		OpenGLGraphicsModule::LogGLError(glGetError());
		UnBindForDraw();
	}

	void OGLRenderTexture::ReadColorPixels(const std::string& attachment, void* buffer, DataType type)
	{
		BindForDraw();
		const uint32_t index = (uint32_t)m_NameToTextureIndex.at(attachment);
		const GLuint format = GLConverter::TO_GLFORMAT.at(m_CreateInfo.Attachments[index].Format);
		const GLenum dataType = GLConverter::TO_GLDATATYPE.at(type);
		glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
		OpenGLGraphicsModule::LogGLError(glGetError());
		glReadPixels(0, 0, m_Width, m_Height, format, dataType, buffer);
		OpenGLGraphicsModule::LogGLError(glGetError());
		UnBindForDraw();
	}

	void OGLRenderTexture::ReadDepthPixel(const glm::ivec2& coord, void* value, DataType type)
	{
		BindForDraw();
		const uint32_t index = (uint32_t)m_NameToTextureIndex.at("Depth");
		const GLuint format = GLConverter::TO_GLFORMAT.at(PixelFormat::PF_Depth);
		const GLenum dataType = GLConverter::TO_GLDATATYPE.at(type);
		glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
		OpenGLGraphicsModule::LogGLError(glGetError());
		glReadPixels(coord.x, coord.y, 1, 1, format, dataType, value);
		OpenGLGraphicsModule::LogGLError(glGetError());
		UnBindForDraw();
	}

	void OGLRenderTexture::Initialize()
	{
		// Create framebuffer
		glGenFramebuffers(1, &m_GLFrameBufferID);
		OpenGLGraphicsModule::LogGLError(glGetError());
		glBindFramebuffer(GL_FRAMEBUFFER, m_GLFrameBufferID);
		OpenGLGraphicsModule::LogGLError(glGetError());

		CreateTextures();
		
		// Initialize the framebuffer
		const size_t attachmentCount = m_CreateInfo.Attachments.size();
		std::vector<GLenum> drawBuffers = std::vector<GLenum>(attachmentCount);
		for (uint32_t i = 0; i < attachmentCount; i++)
		{
			drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
			GLTexture* pTexture = (GLTexture*)m_pTextures[i];
			glFramebufferTexture(GL_FRAMEBUFFER, drawBuffers[i], pTexture->GetID(), 0);
			OpenGLGraphicsModule::LogGLError(glGetError());
		}
		
		if (m_CreateInfo.HasDepth)
		{
			GLTexture* pDepthTexture = (GLTexture*)GetTextureAttachment("Depth");
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, pDepthTexture->GetID(), 0);
			OpenGLGraphicsModule::LogGLError(glGetError());
		}
		if (m_CreateInfo.HasStencil)
		{
			GLTexture* pStencilTexture = (GLTexture*)GetTextureAttachment("Stencil");
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, pStencilTexture->GetID(), 0);
			OpenGLGraphicsModule::LogGLError(glGetError());
		}

		if (attachmentCount > 0)
		{
			glDrawBuffers(attachmentCount, &drawBuffers[0]);
			OpenGLGraphicsModule::LogGLError(glGetError());
		}
		
		// Check if something went wrong
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			m_pOwner->GetEngine()->GetDebug().LogError("There was an error when trying to create a frame buffer!");
			return;
		}

		glBindTexture(GL_TEXTURE_2D, NULL);
		OpenGLGraphicsModule::LogGLError(glGetError());
		glBindFramebuffer(GL_FRAMEBUFFER, NULL);
		OpenGLGraphicsModule::LogGLError(glGetError());
	}

	void OGLRenderTexture::OnResize()
	{
		if (m_GLFrameBufferID != NULL) glDeleteFramebuffers(1, &m_GLFrameBufferID);
		OpenGLGraphicsModule::LogGLError(glGetError());
		//if (m_GLDepthBufferID != NULL) glDeleteRenderbuffers(1, &m_GLDepthBufferID);
		//OpenGLGraphicsModule::LogGLError(glGetError());
		m_GLFrameBufferID = NULL;
		//m_GLDepthBufferID = NULL;

		GPUResourceManager* pResourceManager = m_pOwner->GetEngine()->GetMainModule<GraphicsModule>()->GetResourceManager();
		size_t attachmentCount = m_CreateInfo.Attachments.size();
		for (size_t i = 0; i < attachmentCount; i++)
		{
			pResourceManager->Free(m_pTextures[i]);
		}
		if (m_CreateInfo.HasDepth)
		{
			pResourceManager->Free(m_pTextures[attachmentCount]);
			++attachmentCount;
		}
		if (m_CreateInfo.HasStencil) pResourceManager->Free(m_pTextures[attachmentCount]);
		Initialize();
	}

	void OGLRenderTexture::BindForDraw()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_GLFrameBufferID);
		OpenGLGraphicsModule::LogGLError(glGetError());
		glViewport(0, 0, m_Width, m_Height);
		OpenGLGraphicsModule::LogGLError(glGetError());
	}

	void OGLRenderTexture::BindRead()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GLFrameBufferID);
		OpenGLGraphicsModule::LogGLError(glGetError());
	}
	
	void OGLRenderTexture::UnBindForDraw()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, NULL);
		OpenGLGraphicsModule::LogGLError(glGetError());
	}

	void OGLRenderTexture::UnBindRead()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		OpenGLGraphicsModule::LogGLError(glGetError());
	}
}
