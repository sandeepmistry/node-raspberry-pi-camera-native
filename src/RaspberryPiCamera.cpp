#include <bcm_host.h>

#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_connection.h>

#include "RaspberryPiCamera.h"

Napi::FunctionReference RaspberryPiCamera::constructor;

Napi::Object RaspberryPiCamera::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "RaspberryPiCamera", {
    InstanceMethod("start", &RaspberryPiCamera::Start),
    InstanceMethod("pause", &RaspberryPiCamera::Pause),
    InstanceMethod("resume", &RaspberryPiCamera::Resume),
    InstanceMethod("stop", &RaspberryPiCamera::Stop)
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("Native", func);
  return exports;
}

RaspberryPiCamera::RaspberryPiCamera(const Napi::CallbackInfo& info) : Napi::ObjectWrap<RaspberryPiCamera>(info)  {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  int length = info.Length();

  if (length <= 0 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
  }

  this->_dataCallback = Napi::Persistent(info[0].As<Napi::Function>());
}

Napi::Value RaspberryPiCamera::Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);

  int length = info.Length();

  _width = 1280;
  _height = 720;
  _fps = 30;
  _encoding = MMAL_ENCODING_JPEG;
  _quality = 75;

  if (length > 0 && info[0].IsObject()) {
    Napi::Object options = info[0].ToObject();

    if (options.Has("width") && options.Get("width").IsNumber()) {
      _width = options.Get("width").ToNumber().Int32Value();
    }

    if (options.Has("height") && options.Get("height").IsNumber()) {
      _height = options.Get("height").ToNumber().Int32Value();
    }

    if (options.Has("fps") && options.Get("fps").IsNumber()) {
      _fps = options.Get("fps").ToNumber().Int32Value();
    }

    if (options.Has("encoding") && options.Get("encoding").IsString()) {
      std::string encoding = options.Get("encoding").ToString();

      while (encoding.length() < 4) {
        encoding += ' ';
      }

      _encoding = MMAL_FOURCC(encoding[0], encoding[1], encoding[2], encoding[3]);
    }

    if (options.Has("quality") && options.Get("quality").IsNumber()) {
      _quality = options.Get("quality").ToNumber().Int32Value();
    }
  }

  this->_asyncHandle = new uv_async_t;

  uv_async_init(uv_default_loop(), this->_asyncHandle, (uv_async_cb)RaspberryPiCamera::AsyncCallback);
  uv_mutex_init(&this->_bufferQueueMutex);

  this->_asyncHandle->data = this;

  // create camera
  if (mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &_camera)!= MMAL_SUCCESS) {
    // Failed to create camera component
    Napi::TypeError::New(env, "Failed to create camera!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!_camera->output_num) {
    // Camera doesn't have output ports
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Camera has no output ports!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Enable the camera control
  if (mmal_port_enable(_camera->control, RaspberryPiCamera::CameraControlCallback)!= MMAL_SUCCESS) {
    // Unable to enable control port
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to enable camera control!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // set camera config
  MMAL_PARAMETER_CAMERA_CONFIG_T cameraConfig;

  cameraConfig.hdr.id = MMAL_PARAMETER_CAMERA_CONFIG;
  cameraConfig.hdr.size = sizeof(cameraConfig);
  cameraConfig.max_stills_w = _width;
  cameraConfig.max_stills_h = _height;
  cameraConfig.stills_yuv422 = 0;
  cameraConfig.one_shot_stills = 0;
  cameraConfig.max_preview_video_w = _width;
  cameraConfig.max_preview_video_h = _height;
  cameraConfig.num_preview_video_frames = 1;
  cameraConfig.stills_capture_circular_buffer_height = 0;
  cameraConfig.fast_preview_resume = 0;
  cameraConfig.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;

  if (mmal_port_parameter_set(_camera->control, &cameraConfig.hdr) != MMAL_SUCCESS) {
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to set camera config!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // setup port format
  MMAL_ES_FORMAT_T *format = _camera->output[0]->format;

  format->es->video.width = VCOS_ALIGN_UP(_width, 32);
  format->es->video.height = VCOS_ALIGN_UP(_height, 16);
  format->es->video.crop.x = 0;
  format->es->video.crop.y = 0;
  format->es->video.crop.width = _width;
  format->es->video.crop.height = _height;
  format->es->video.frame_rate.num = _fps;
  format->es->video.frame_rate.den = 1;

  if (mmal_port_format_commit(_camera->output[0]) != MMAL_SUCCESS) {
    // Couldn't set video port format
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to set camera format!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  //enable the camera
  if (mmal_component_enable(_camera) != MMAL_SUCCESS) {
    // Couldn't enable camera
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to enable camera capture!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // create image encoder
  if (mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &_encoder) != MMAL_SUCCESS) {
    mmal_component_disable(_camera);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to create image encoder!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // setup encoder format
  mmal_format_copy(_encoder->output[0]->format, _encoder->input[0]->format);
  _encoder->output[0]->format->encoding = _encoding;

  _encoder->output[0]->buffer_size = _encoder->output[0]->buffer_size_recommended;
  _encoder->output[0]->buffer_num = _encoder->output[0]->buffer_num_recommended;

  if (mmal_port_format_commit(_encoder->output[0]) != MMAL_SUCCESS) {
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to set encoder format!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (mmal_port_parameter_set_uint32(_encoder->output[0], MMAL_PARAMETER_JPEG_Q_FACTOR, _quality) != MMAL_SUCCESS) {
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to set encoder JPEG quality factor!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (mmal_port_parameter_set_uint32(_encoder->output[0], MMAL_PARAMETER_JPEG_RESTART_INTERVAL, 0) != MMAL_SUCCESS) {
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to set encoder JPEG restart interval!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // enable encoder
  if (mmal_component_enable(_encoder) != MMAL_SUCCESS) {
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to enable encoder!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // create buffer pool
  _bufferPool = mmal_port_pool_create(_encoder->output[0], _encoder->output[0]->buffer_num, _encoder->output[0]->buffer_size);
  if (!_bufferPool) {
    mmal_component_disable(_encoder);
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to create buffer pool!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (mmal_connection_create(&_encoderConnection, _camera->output[0], _encoder->input[0], MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT) != MMAL_SUCCESS) {
    mmal_port_pool_destroy(_encoder->output[0], _bufferPool);
    mmal_component_disable(_encoder);
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to create camera encoder connection!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (mmal_connection_enable(_encoderConnection) != MMAL_SUCCESS) {
    mmal_connection_destroy(_encoderConnection);
    mmal_port_pool_destroy(_encoder->output[0], _bufferPool);
    mmal_component_disable(_encoder);
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to enable camera encoder connection!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // enable the port and hand it the callback
  _encoder->output[0]->userdata = (struct MMAL_PORT_USERDATA_T *)this;

  if (mmal_port_enable(_encoder->output[0], RaspberryPiCamera::BufferCallback) != MMAL_SUCCESS) {
    // Failed to set video buffer callback
    mmal_port_pool_destroy(_encoder->output[0], _bufferPool);
    mmal_component_disable(_encoder);
    mmal_component_disable(_camera);
    mmal_component_destroy(_encoder);
    mmal_component_destroy(_camera);
    Napi::TypeError::New(env, "Failed to enable encoder output port!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  //send all the buffers in our pool to the video port ready for use
  int queueLength = mmal_queue_length(_bufferPool->queue);

  for (int i = 0; i < queueLength; i++) {
    MMAL_BUFFER_HEADER_T* buffer = mmal_queue_get(_bufferPool->queue);

    if (!buffer) {
      // Unable to get a required buffer %d from pool queue
      mmal_port_pool_destroy(_encoder->output[0], _bufferPool);
      mmal_component_disable(_encoder);
      mmal_component_disable(_camera);
      mmal_component_destroy(_encoder);
      mmal_component_destroy(_camera);
      Napi::TypeError::New(env, "Failed to get buffer from pool!").ThrowAsJavaScriptException();
      return env.Undefined();
    }

    if (mmal_port_send_buffer(_encoder->output[0], buffer) != MMAL_SUCCESS) {
      mmal_port_pool_destroy(_encoder->output[0], _bufferPool);
      mmal_component_disable(_encoder);
      mmal_component_disable(_camera);
      mmal_component_destroy(_encoder);
      mmal_component_destroy(_camera);
      Napi::TypeError::New(env, "Failed to send buffer to encoder output!").ThrowAsJavaScriptException();
      return env.Undefined();
    }
  }

  return env.Undefined();
}

Napi::Value RaspberryPiCamera::Pause(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (mmal_port_parameter_set_boolean(_camera->output[0], MMAL_PARAMETER_CAPTURE, 0) != MMAL_SUCCESS) {
    Napi::TypeError::New(env, "Failed to disable camera capture!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value RaspberryPiCamera::Resume(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (mmal_port_parameter_set_boolean(_camera->output[0], MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS) {
    Napi::TypeError::New(env, "Failed to enable camera capture!").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value RaspberryPiCamera::Stop(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  mmal_port_disable(_encoder->output[0]);
  mmal_port_pool_destroy(_encoder->output[0], _bufferPool);
  mmal_connection_destroy(_encoderConnection);
  mmal_component_disable(_encoder);
  mmal_component_disable(_camera);
  mmal_component_destroy(_encoder);
  mmal_component_destroy(_camera);

  uv_close((uv_handle_t*)this->_asyncHandle, (uv_close_cb)RaspberryPiCamera::AsyncCloseCallback);
  uv_mutex_destroy(&this->_bufferQueueMutex);

  return env.Undefined();
}

void RaspberryPiCamera::_processBufferQueue() {
  uv_mutex_lock(&this->_bufferQueueMutex);

  Napi::Env env = this->Env();
  Napi::HandleScope scope(env);

  while (!this->_bufferQueue.empty()) {
    MMAL_BUFFER_HEADER_T* buffer = this->_bufferQueue.front();
    this->_bufferQueue.pop();

    Napi::Buffer<uint8_t> data = Napi::Buffer<uint8_t>::Copy(env, (uint8_t*)(buffer->data + buffer->offset), buffer->length);

    this->_dataCallback.MakeCallback(env.Global(), { data });

    mmal_port_send_buffer(_encoder->output[0], buffer);
  }

  uv_mutex_unlock(&this->_bufferQueueMutex);
}

void RaspberryPiCamera::AsyncCallback(uv_async_t* handle)
{
  RaspberryPiCamera* raspberryPiCamera = (RaspberryPiCamera*)handle->data;

  raspberryPiCamera->_processBufferQueue();
}

void RaspberryPiCamera::AsyncCloseCallback(uv_async_t* handle)
{
  delete handle;
}

void RaspberryPiCamera::CameraControlCallback(MMAL_PORT_T* port, MMAL_BUFFER_HEADER_T* buffer) {
  mmal_buffer_header_release(buffer);
}

void RaspberryPiCamera::BufferCallback(MMAL_PORT_T* port, MMAL_BUFFER_HEADER_T* buffer) {
  RaspberryPiCamera* raspberryPiCamera = (RaspberryPiCamera*)port->userdata;

  uv_mutex_lock(&raspberryPiCamera->_bufferQueueMutex);
  raspberryPiCamera->_bufferQueue.push(buffer);
  uv_mutex_unlock(&raspberryPiCamera->_bufferQueueMutex);

  uv_async_send(raspberryPiCamera->_asyncHandle);
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  bcm_host_init();

  return RaspberryPiCamera::Init(env, exports);
}

NODE_API_MODULE(addon, InitAll);
