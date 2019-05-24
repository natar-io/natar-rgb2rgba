/**
 * @Author: PALARD Nicolas <nclsp>
 * @Date:   2019-04-11T10:48:56+02:00
 * @Email:  palard@rea.lity.tech
 * @Project: Natar.io
 * @Last modified by:   nclsp
 * @Last modified time: 2019-04-11T10:49:29+02:00
 * @Copyright: RealityTech 2018-2019
 */

#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <RedisImageHelper.hpp>

struct contextData {
    uint width;
    uint height;
    uint channels;
    RedisImageHelperSync* clientSync;
};

std::string redisInputKey = "";

void onImagePublished(redisAsyncContext* c, void* rep, void* privdata) {
    redisReply *reply = (redisReply*) rep;
    if  (reply == NULL) { return; }
    if (reply->type != REDIS_REPLY_ARRAY || reply->elements != 3) {
        std::cerr << "Error: Bad reply format." << std::endl;
        return;
    }

    struct contextData* data = static_cast<struct contextData*>(privdata);
    if (data == NULL) {
        std::cerr << "Error: Could not retrieve context data from private data." << std::endl;
        return;
    }
    uint width = data->width;
    uint height = data->height;
    uint channels = data->channels;
    RedisImageHelperSync* clientSync = data->clientSync;

    // Getting image from Natar
    Image* image = clientSync->getImage(width, height, channels, redisInputKey);
    if (image == NULL) {
        std::cerr << "Error: Could not retrieve image from data." << std::endl;
        return;
    }
    // Converting Natar image to OpenCV
    cv::Mat rgbaImage = cv::Mat(image->height(), image->width(), CV_8UC4, image->data());
    cv::Mat rgbImage;
    cv::cvtColor(rgbaImage, rgbImage, cv::COLOR_RGBA2RGB);

    // Converting OpenCV image to Natar
    Image* outputImage = new Image(rgbImage.cols, rgbImage.rows, rgbImage.channels(), rgbImage.data);
    // Setting image into Natar
    clientSync->setImage(outputImage, redisInputKey + ":rgb");
    clientSync->publishString("json", redisInputKey + ":rgb");
    delete image;
}


int main(int argc, char** argv) {
    std::string redisHost = "127.0.0.1";
    int redisPort = 6379;

    if (argc != 2) {
        std::cerr << "Missing commandline argument: ./natar-rgba2rgb rgba-key";
        return EXIT_FAILURE;
    }
    redisInputKey = argv[1];

    RedisImageHelperSync clientSync(redisHost, redisPort, "");
    if (!clientSync.connect()) {
        std::cerr << "Cannot connect to redis server. Please ensure that a redis server is up and running." << std::endl;
        return EXIT_FAILURE;
    }

    // Getting image data from Natar (every image has its associated width, height, channels keys)
    struct contextData data;
    data.width = clientSync.getInt(redisInputKey + ":width");
    data.height = clientSync.getInt(redisInputKey + ":height");
    data.channels = clientSync.getInt(redisInputKey + ":channels");
    if (data.channels != 4) {
        std::cerr << "Could not convert the input image because it has only " << data.channels << " channels while it requires 4" << std::endl;
        return EXIT_FAILURE;
    }
    data.clientSync = &clientSync;

    RedisImageHelperAsync clientAsync(redisHost, redisPort, redisInputKey);
    if (!clientAsync.connect()) {
        std::cerr << "Cannot connect to redis server. Please ensure that a redis server is up and running." << std::endl;
        return EXIT_FAILURE;
    }
    clientAsync.subscribe(redisInputKey, onImagePublished, static_cast<void*>(&data));
}
