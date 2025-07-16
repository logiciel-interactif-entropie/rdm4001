#pragma once
#include <memory>

#include "base_context.hpp"
#include "base_types.hpp"

namespace rdm::gfx {
class Engine;

/**
 * @brief This is the rendering device to submit commands directly to the
 * underlying graphics API.
 *
 * If you are accessing in threads other then the Render job thread, please lock
 * the underlying context.
 */
class BaseDevice {
  friend class Engine;

  BaseContext* context;
  Engine* engine;

 public:
  BaseDevice(BaseContext* context);
  virtual ~BaseDevice() {};

  Engine* getEngine() { return engine; }

  enum DepthStencilState {
    Disabled,
    Always,
    Never,
    Less,
    Equal,
    LEqual,
    Greater,
    NotEqual,
    GEqual,
  };

  enum BlendState {
    DDisabled,
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    Constant,
    OneMinusConstant,
    ConstantAlpha,
    OneMinusConstantAlpha,
  };

  enum CullState {
    FrontCCW,
    BackCCW,
    FrontCW,  // DEFAULT
    BackCW,
    None,
  };

  virtual void setDepthState(DepthStencilState s) = 0;
  virtual void setStencilState(DepthStencilState s) = 0;
  virtual void setBlendState(BlendState a, BlendState b) = 0;
  virtual void setCullState(CullState s) = 0;

  virtual void readPixels(int x, int y, int w, int h, void* d) = 0;
  /**
   * @brief Clears the color buffer of the current framebuffer.
   *
   * @param r Red
   * @param g Green
   * @param b Blue
   * @param a Alpha
   */
  virtual void clear(float r, float g, float b, float a) = 0;

  /**
   * @brief Clears the depth buffer of the current framebuffer.
   *
   * @param d Depth
   */
  virtual void clearDepth(float d = 1.0) = 0;

  /**
   * @brief Viewport to render within.
   *
   * @param x X.coordinate. On OpenGL, 0 is the left-most pixel on the rendering
   * window.
   * @param y Y coordinate. On OpenGL, 0 is the bottom most pixel on the
   * rendering window.
   * @param w Width of viewport. On OpenGL, higher values make the width of your
   * viewport get closer to the right most pixel on the rendering window.
   * @param h Height of viewport. On OpenGL, higher values make the height of
   * your viewport get closer to the top most pixel on the rendering window.
   */
  virtual void viewport(int x, int y, int w, int h) = 0;

  /**
   * @brief The type of mesh that will be drawn.
   *
   */
  enum DrawType {
    /**
     * @brief Draw individual points. 1 element is 1 point.
     *
     */
    Points,
    /**
     * @brief Draw lines. 1 element is 2 points.
     *
     */
    Lines,
    /**
     * @brief Draw triangles. 1 element is 3 points.
     *
     */
    Triangles,
  };

  /**
   * @brief Draws a buffer.
   * You will want to bind a BaseArrayPointers by using BaseArrayPointers::bind.
   *
   * @param base The base buffer (buffer to be drawn.)
   * @param type The type of data within the buffer. On OpenGL, this is not
   * useful when drawing a BaseBuffer::Array buffer, because it uses
   * glDrawArrays.
   * @param dtype The drawing type of the buffer. See DrawType... duh.
   * @param count The number of elements/vertices to draw.
   * @param pointer The first element/vertex to draw within base. If using
   * Array, pointer should be the index (not offset) of the first vertex to
   * draw.
   */
  virtual void draw(BaseBuffer* base, DataType type, DrawType dtype,
                    size_t count, void* pointer = 0) = 0;

  /**
   * @brief Binds a framebuffer
   *
   * You must use BaseDevice::unbindFramebuffer when you are done with the
   * framebuffer.
   *
   * @param buffer The framebuffer
   * @return void* Pass this to BaseDevice::unbindFramebuffer
   */
  virtual void* bindFramebuffer(BaseFrameBuffer* buffer) = 0;

  /**
   * @brief Unbinds a framebuffer
   *
   * @param p Return value of BaseDevice::bindFramebuffer
   */
  virtual void unbindFramebuffer(void* p) = 0;

  /**
   * @brief Create a Texture object
   *
   * You likely want to use TextureCache::getOrLoad. You can get a TextureCache*
   * by using Engine::getTextureCache
   *
   * @return std::unique_ptr<BaseTexture>
   */
  virtual std::unique_ptr<BaseTexture> createTexture() = 0;
  /**
   * @brief Create a Program object
   *
   * Only create Programs if you know what you're doing. Otherwise, use a
   * Material.
   *
   * @return std::unique_ptr<BaseProgram>
   */
  virtual std::unique_ptr<BaseProgram> createProgram() = 0;
  /**
   * @brief Create a Buffer object
   *
   * @return std::unique_ptr<BaseBuffer>
   */
  virtual std::unique_ptr<BaseBuffer> createBuffer() = 0;
  /**
   * @brief Create a Array Pointers object
   *
   * @return std::unique_ptr<BaseArrayPointers>
   */
  virtual std::unique_ptr<BaseArrayPointers> createArrayPointers() = 0;
  /**
   * @brief Create a Frame Buffer object
   *
   * @return std::unique_ptr<BaseFrameBuffer>
   */
  virtual std::unique_ptr<BaseFrameBuffer> createFrameBuffer() = 0;

  virtual void targetAttachments(BaseFrameBuffer::AttachmentPoint* attachments,
                                 int count) = 0;

  /**
   * @brief Beginning of imgui frame
   *
   * @todo port imgui itself to engine system
   */
  virtual void startImGui() = 0;
  virtual void stopImGui() = 0;

  virtual void dbgPushGroup(std::string message) = 0;
  virtual void dbgPopGroup() = 0;

  virtual ShaderBinaryType getPreferedShaderType() { return GlslPreprocessed; }
};
};  // namespace rdm::gfx
