# node-raspberry-pi-camera-native

Use your [Raspberry Pi Camera Module](https://www.raspberrypi.org/documentation/hardware/camera/README.md) with [Node.js](https://nodejs.org)

**NOTE:** Currently only supports still image capture.

## Prerequisites

 * Hardware
   * Raspberry Pi
   * [Raspberry Pi Camera module](https://www.raspberrypi.org/documentation/hardware/camera/README.md)

 * Software
   * [Raspberry Pi camera enabled](https://www.raspberrypi.org/documentation/configuration/camera.md)
   * Node.js 8 or later installed

## Install

```
npm install raspberry-pi-camera-native
```

## Usage

```javascript
// require module
const raspberryPiCamera = require('raspberry-pi-camera-native');

// add frame data event listener
raspberryPiCamera.on('frame', (frameData) => {
  // frameData is a Node.js Buffer
  // ...
});

// start capture
raspberryPiCamera.start();
```

### Events

#### Data

Listen for raw data events (partial frame data), `data` is a [Node.js Buffer](https://nodejs.org/dist/latest/docs/api/buffer.html)

```javascript
raspberryPiCamera.on('data', callback(data));
```

#### Frame

Listen for frame events (full frame data), `frameData` is a [Node.js Buffer](https://nodejs.org/dist/latest/docs/api/buffer.html)

```javascript
raspberryPiCamera.on('frame', callback(frameData));
```

### Actions

#### Start Capture

```javascript
raspberryPiCamera.start(options, callback);
```

Options is a object, with the following defaults:
```javascript
{
  width: 1280,
  height: 720,
  fps: 30,
  encoding: 'JPEG',
  quality: 75
}
```

Supported values:
 * `width`: `32` to `2592` (v1 camera) or `3280` (v2 camera)
 * `height`: `16` to `1944` (v1 camera) or `2464` (v2 camera)
 * `fps`: `1` to `90`
 * `encoding`: `'JPEG'` (hardware accelerated), `'GIF'`, `'PNG'`, `'PPM'`, `'TGA'`, `'BMP'` (see [mmal_encodings.h](https://github.com/raspberrypi/userland/blob/master/interface/mmal/mmal_encodings.h) for others)
 * `quality`: 1 - 100 (JPEG encoding quality)

#### Pause Capture

```javascript
raspberryPiCamera.pause(callback);
```

#### Resume Capture

```javascript
raspberryPiCamera.resume(callback);
```

#### Stop Capture

```javascript
raspberryPiCamera.stop(callback);
```
