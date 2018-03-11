#ifndef RASPBERRY_PI_CAMERA_H
#define RASPBERRY_PI_CAMERA_H

#include <queue>

#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_parameters_camera.h>

#include <uv.h>

#include <napi.h>

class RaspberryPiCamera : public Napi::ObjectWrap<RaspberryPiCamera> {

public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  RaspberryPiCamera(const Napi::CallbackInfo& info);

  Napi::Value Start(const Napi::CallbackInfo& info);
  Napi::Value Pause(const Napi::CallbackInfo& info);
  Napi::Value Resume(const Napi::CallbackInfo& info);
  Napi::Value Stop(const Napi::CallbackInfo& info);

private:
  static Napi::FunctionReference constructor;

  void _processBufferQueue();

  static void AsyncCallback(uv_async_t* handle);
  static void AsyncCloseCallback(uv_async_t* handle);

  static void CameraControlCallback(MMAL_PORT_T* port, MMAL_BUFFER_HEADER_T* buffer);
  static void BufferCallback(MMAL_PORT_T* port, MMAL_BUFFER_HEADER_T* buffer);

private:
  MMAL_COMPONENT_T* _camera;
  MMAL_COMPONENT_T* _encoder;
  MMAL_POOL_T* _bufferPool;
  MMAL_CONNECTION_T *_encoderConnection;
  int _width;
  int _height;
  int _fps;
  MMAL_FOURCC_T _encoding;
  int _quality;

  uv_async_t* _asyncHandle;
  uv_mutex_t _bufferQueueMutex;
  std::queue<MMAL_BUFFER_HEADER_T*> _bufferQueue;

  Napi::FunctionReference _dataCallback;
};


#endif
