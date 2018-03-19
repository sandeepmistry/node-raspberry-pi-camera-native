//
// This example starts camera capture the current capture frames per second (fps) and
// 1 minute load average to the console. It can be used to benchmark performance.
//

const os = require('os');

const raspberryPiCamera = require('../index'); // or require('raspberry-pi-camera-native');

let fps = 0;

let fpsInterval = setInterval(() => {
  const loadAvg = (os.loadavg()[0] * 100);

  console.log('fps %d, load average = %f%%', fps, loadAvg.toFixed(1));
  fps = 0;
}, 1000);

raspberryPiCamera.on('frame', (data) => {
  fps++;
});

raspberryPiCamera.start({
  width: 1920,
  height: 1080,
  fps: 30,
  quality: 80,
  encoding: 'JPEG'
});
