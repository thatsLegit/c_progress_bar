#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <curl/curl.h>

const int PROGRESS_BAR_LENGTH = 30;

// A better prediction on exp_bytes_per_url can be made by sending head requests
// prior to the actual request, as it often sends back a content-length header.

typedef struct {
    long total_bytes; /* How many dl bytes so far */
    long total_expected; /* How many bytes we think we are getting */
    double exp_bytes_per_url;
    long current_bytes; /* Number of bytes in the current dl */
    int urls_so_far;
    int total_urls; /* Total number of urls */
} statusinfo;

// Implementing exponential weighted moving average
// alpha value: how much we are going to predict based on the most recent thing
const double PREDICT_WEIGHT = 0.4;

// 1. if last_prediction == actual, return value is the same
// 2. if PREDICT_WEIGHT ~ 1, the prediction will depend much more on actual
// 3. if PREDICT_WEIGHT ~0, the prediction will depend much more on last_prediction
double predict_next(double last_prediction, double actual) {
    return last_prediction * (1 - PREDICT_WEIGHT) + actual * PREDICT_WEIGHT;
}

void updateBar(int percentage_done, statusinfo* sinfo) {
    int num_chars = percentage_done * PROGRESS_BAR_LENGTH / 100;
    printf("\r[");
    for(int i = 0; i < num_chars; i++) {
        printf("#");
    }
    for(int i = 0; i < PROGRESS_BAR_LENGTH - num_chars; i++) {
        printf(" ");
    }
    printf("] %d%% Done (%ld MB)", percentage_done, sinfo->total_bytes / 1000000);
    fflush(stdout);
}

size_t got_data(char* buffer, size_t itemSize, size_t numItems, void* sinfo) {
    statusinfo* status = sinfo;
    size_t bytes = itemSize * numItems;

    status->total_bytes += bytes;
    status->current_bytes += bytes;

    int urls_left = status->total_urls - status->urls_so_far;

    long estimate_current = status->exp_bytes_per_url;

    // wrong prediction (we are higher than expected)
    if(status->current_bytes > status->exp_bytes_per_url) {
        estimate_current = status->current_bytes * 4/3; /* adding 33% */
    }

    long guess_next_prediction = predict_next(status->exp_bytes_per_url, estimate_current);

    long prediction_total = status->total_bytes /* how much already done */
    + (estimate_current - status->current_bytes) /* how much left on this dl */
    + (urls_left - 1) * guess_next_prediction;

    long percent_done = status->total_bytes * 100 / prediction_total;

    updateBar(percent_done, sinfo);
    return bytes;
}

bool downloadUrl(char* url, statusinfo* sinfo) {
    CURL *curl = curl_easy_init();

    if(!curl) return false;

    // set options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, got_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, sinfo);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    // do the download
    CURLcode result = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    return (result == CURLE_OK);
}

int main() {
    char* urls[] = {
        "https://cdn.pixabay.com/photo/2022/03/06/05/30/clouds-7050884_1280.jpg",
        "https://cdimage.debian.org/cdimage/release/current/amd64/iso-cd/debian-11.2.0-amd64-netinst.iso",
        "https://cdn.pixabay.com/photo/2020/03/09/17/51/narcis-4916584_1280.jpg",
        "https://cdn.pixabay.com/photo/2022/03/01/20/58/peace-genius-7042013_1280.jpg",
        "https://cdn.pixabay.com/photo/2017/06/05/07/58/butterfly-2373175_1280.png"
    };
    int num_urls = sizeof(urls) / sizeof(urls[0]);

    statusinfo sinfo;
    sinfo.total_bytes = 0;
    sinfo.total_urls = num_urls;
    sinfo.urls_so_far = 0;
    // if the initial prediction is too low, the bar will grow very fast then slow
    // down quickly, so it's better to be a bit pessimistic.
    sinfo.exp_bytes_per_url = 100000000.0; /* 100MB */

    updateBar(0, &sinfo);
    for (int i = 0; i < num_urls; i++) {
        sinfo.current_bytes = 0;
        downloadUrl(urls[i], &sinfo);
        sinfo.urls_so_far++;
        sinfo.exp_bytes_per_url = predict_next(sinfo.exp_bytes_per_url, sinfo.current_bytes);
    }
    updateBar(100, &sinfo);
    printf("\n");
    return 0;
}