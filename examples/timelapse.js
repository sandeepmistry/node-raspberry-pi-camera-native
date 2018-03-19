//
// This example starts camera capture saves each frame to a file on disk.
//

const fs = require('fs');

const raspberryPiCamera = require('../index'); // or require('raspberry-pi-camera-native');

let count = 0;

raspberryPiCamera.on('frame', (frameData) => {
  const filename = 'img' + (count + '').padStart(3, '0') + '.jpg';

  console.log('writing file: ', filename);

  fs.writeFile(filename, frameData, (err) => {
    if (err) {
      throw err;
    }

    count++;
  });
});

raspberryPiCamera.start({
  width: 1920,
  height: 1080,
  fps: 1,
  quality: 80,
  encoding: 'JPEG'
});
