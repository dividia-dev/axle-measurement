#!/usr/bin/env node
/**
 * Downloads a royalty-free test video for development.
 * Uses a short clip from the Blender Foundation (Big Buck Bunny).
 * This is used as a stand-in for the RTSP camera feed during development.
 */

const https = require('https');
const fs = require('fs');
const path = require('path');

const VIDEO_URL = 'https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/720/Big_Buck_Bunny_720_10s_1MB.mp4';
const OUTPUT_PATH = path.join(__dirname, '..', 'public', 'test-video.mp4');

if (fs.existsSync(OUTPUT_PATH)) {
    console.log('Test video already exists at', OUTPUT_PATH);
    process.exit(0);
}

console.log('Downloading test video...');
console.log('Source:', VIDEO_URL);

function download(url, dest) {
    return new Promise((resolve, reject) => {
        const file = fs.createWriteStream(dest);
        https.get(url, (response) => {
            if (response.statusCode === 301 || response.statusCode === 302) {
                file.close();
                fs.unlinkSync(dest);
                return download(response.headers.location, dest).then(resolve).catch(reject);
            }
            if (response.statusCode !== 200) {
                file.close();
                fs.unlinkSync(dest);
                reject(new Error(`HTTP ${response.statusCode}`));
                return;
            }
            response.pipe(file);
            file.on('finish', () => {
                file.close();
                resolve();
            });
        }).on('error', (err) => {
            file.close();
            fs.unlinkSync(dest);
            reject(err);
        });
    });
}

download(VIDEO_URL, OUTPUT_PATH)
    .then(() => console.log('Downloaded to', OUTPUT_PATH))
    .catch((err) => {
        console.error('Failed to download test video:', err.message);
        console.log('\nYou can manually place any .mp4 file at:');
        console.log(OUTPUT_PATH);
        process.exit(1);
    });
