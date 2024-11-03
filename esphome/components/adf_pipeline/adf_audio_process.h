#pragma once

#ifdef USE_ESP_IDF

#include "esphome/core/component.h"

#include "adf_audio_element.h"
#include "equalizer.h"

namespace esphome {
namespace esp_adf {

class ADFPipelineProcessElement : public ADFPipelineElement {
 public:
  AudioPipelineElementType get_element_type() const { return AudioPipelineElementType::AUDIO_PIPELINE_PROCESS; }
};

class ADFResampler : public ADFPipelineProcessElement {
 public:
  const std::string get_name() override { return "Resampler"; }

 protected:
  bool init_adf_elements_() override;
  void on_settings_request(AudioPipelineSettingsRequest &request) override;

  int src_rate_{16000};
  int dst_rate_{16000};
  int src_num_channels_{2};
  int dst_num_channels_{2};
  int src_bit_depth_{16};

  audio_element_handle_t sdk_resampler_;
};

class ADFEqualizer : public ADFPipelineProcessElement, public Component {
 public:

  const std::string get_name() override { return "Equalizer"; }

  void set_eq(int index, int value_gain);

  // ESPHome Component implementations
  void setup() override {}

 protected:
  bool init_adf_elements_() override;
  void on_settings_request(AudioPipelineSettingsRequest &request) override;

  int rate_{16000};
  int num_channels_{2};

  audio_element_handle_t sdk_equalizer_;
};

}  // namespace esp_adf
}  // namespace esphome
#endif
