#include "esp_adf_microphone.h"

#ifdef USE_ESP_IDF

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "esp_adf.microphone";

void ADFMicrophone::setup() {}

void ADFMicrophone::dump_config() {}

void ADFMicrophone::start() {
  pipeline.start();
  this->state_ = microphone::STATE_STARTING;
}

void ADFMicrophone::stop() {
  pipeline.stop();
  this->state_ = microphone::STATE_STOPPING;
}

size_t ADFMicrophone::read(int16_t *buf, size_t len) { return pcm_stream_.stream_read((char *) buf, len); }

void ADFMicrophone::on_pipeline_state_change(PipelineState state) {
  switch (state) {
    case PipelineState::STARTING:
      this->state_ = microphone::STATE_STARTING;
    case PipelineState::RUNNING:
      this->state_ = microphone::STATE_RUNNING;
      break;
    case PipelineState::STOPPING:
      this->state_ = microphone::STATE_STOPPING;
    case PipelineState::UNAVAILABLE:
    case PipelineState::STOPPED:
      this->state_ = microphone::STATE_STOPPED;
      break;
    case PipelineState::PAUSED:
      ESP_LOGI(TAG, "pipeline paused");
      this->state_ = microphone::STATE_STOPPED;
      break;
    case PipelineState::PAUSING:
    case PipelineState::RESUMING:
    case PipelineState::PREPARING:
      break;
  }
}

}  // namespace esp_adf
}  // namespace esphome
#endif
