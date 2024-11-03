#include "adf_audio_process.h"


#ifdef USE_ESP_IDF
#include "adf_pipeline.h"

#include <filter_resample.h>
#include "sdk_ext.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "esp_audio_processors";

typedef struct rsp_filter {
    resample_info_t *resample_info;
    unsigned char *out_buf;
    unsigned char *in_buf;
    void *rsp_hd;
    int in_offset;
    int8_t flag;  //the flag must be 0 or 1. 1 means the parameter in `resample_info_t` changed. 0 means the parameter in `resample_info_t` is original.
} rsp_filter_t;


bool ADFResampler::init_adf_elements_(){
    rsp_filter_cfg_t rsp_cfg = {
      .src_rate = this->src_rate_,
      .src_ch = this->src_num_channels_,
      .dest_rate = this->dst_rate_,
      .dest_bits = 16,
      .dest_ch = this->dst_num_channels_,
      .src_bits = this->src_bit_depth_,
      .mode = RESAMPLE_DECODE_MODE,
      .max_indata_bytes = RSP_FILTER_BUFFER_BYTE,
      .out_len_bytes = RSP_FILTER_BUFFER_BYTE,
      .type = ESP_RESAMPLE_TYPE_AUTO,
      .complexity = 2,
      .down_ch_idx = 0,
      .prefer_flag = ESP_RSP_PREFER_TYPE_SPEED,
      .out_rb_size = RSP_FILTER_RINGBUFFER_SIZE,
      .task_stack = RSP_FILTER_TASK_STACK,
      .task_core = RSP_FILTER_TASK_CORE,
      .task_prio = RSP_FILTER_TASK_PRIO,
      .stack_in_ext = true,
  };
  this->sdk_resampler_= rsp_filter_init(&rsp_cfg);
  sdk_audio_elements_.push_back(this->sdk_resampler_);
  sdk_element_tags_.push_back("resampler");
  return true;
}

void ADFResampler::on_settings_request(AudioPipelineSettingsRequest &request){

  bool settings_changed = false;
  if( request.sampling_rate > -1 ){
    if( request.sampling_rate != this->src_rate_ )
    {
      this->src_rate_ = request.sampling_rate;
      settings_changed = true;
    }
    uint32_t dst_rate = request.final_sampling_rate > -1 ? request.final_sampling_rate : this->src_rate_;
    if( dst_rate != this->dst_rate_ )
    {
      this->dst_rate_ = dst_rate;
      settings_changed = true;
    }
  }
  if( request.number_of_channels > -1 ){
    if( request.number_of_channels != this->src_num_channels_ )
    {
      this->src_num_channels_ = request.number_of_channels;
      settings_changed = true;
    }
    uint32_t dst_num_channels = request.final_number_of_channels > -1 ? request.final_number_of_channels : this->src_num_channels_;
    if( dst_num_channels != this->dst_num_channels_ )
    {
      this->dst_num_channels_ = dst_num_channels;
      settings_changed = true;
    }
  }

  if( request.bit_depth > -1 ){
    if( request.bit_depth != this->src_bit_depth_ )
    {
      this->src_bit_depth_ = request.bit_depth;
      settings_changed = true;
    }
  }

  if ( request.final_bit_depth > -1 && request.final_bit_depth != 16 ){
    request.bit_depth = 16;
    request.final_bit_depth = 16;
    request.requested_by = this;
    this->pipeline_->request_settings(request);
  }



  if( this->sdk_resampler_ && settings_changed)
  {
    if( audio_element_get_state(this->sdk_resampler_) == AEL_STATE_RUNNING)
    {
      audio_element_stop(this->sdk_resampler_);
      audio_element_wait_for_stop(this->sdk_resampler_);
    }
    ADFPipelineElement* el = request.requested_by;
    if( el != nullptr ){
     esph_log_d(TAG, "Received request from: %s", el->get_name().c_str());
    }
    else {
    esph_log_d(TAG, "Called from invalid caller");
    }
    esph_log_d(TAG, "New settings: SRC: rate: %d, ch: %d bits: %d, DST: rate: %d, ch: %d, bits %d", this->src_rate_, this->src_num_channels_, this->src_bit_depth_, this->dst_rate_, this->dst_num_channels_, 16);
    rsp_filter_t *filter = (rsp_filter_t *)audio_element_getdata(this->sdk_resampler_);
    resample_info_t &resample_info = *(filter->resample_info);
    resample_info.src_rate = this->src_rate_;
    resample_info.dest_rate = this->dst_rate_;
    resample_info.src_ch = this->src_num_channels_;
    resample_info.dest_ch = this->dst_num_channels_;
    resample_info.src_bits = this->src_bit_depth_;
    resample_info.dest_bits = 16;
  }

  esph_log_d(TAG, "Current settings: SRC: rate: %d, ch: %d bits: %d, DST: rate: %d, ch: %d, bits %d", this->src_rate_, this->src_num_channels_, this->src_bit_depth_, this->dst_rate_, this->dst_num_channels_, 16);
}

bool ADFEqualizer::init_adf_elements_(){
  // Defined in https://github.com/espressif/esp-adf-libs/blob/master/esp_codec/include/codec/equalizer.h
  equalizer_cfg_t eq_cfg = DEFAULT_EQUALIZER_CONFIG();
  // Per https://github.com/espressif/esp-adf/blob/master/examples/audio_processing/pipeline_equalizer/main/equalizer_example.c
  // ESP-IDF supports 10 bands, and to hold coefficients for up to 2 channels we need a 20-item array
  // I think in mono mode only first 10 are used and rest just are placeholders so that we have memory in case we need it
  // -13 is the default gain per docs
  int set_gain[] = { -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13};
  eq_cfg.set_gain = set_gain;
  eq_cfg.channel = this->num_channels_;
  eq_cfg.samplerate = this->rate_;

  this->sdk_equalizer_ = equalizer_init(&eq_cfg);
  // Presumably this supposed to be applied by equalizer_init based on sdk_equalizer_config_
  // But let's do it once again just in case...
  equalizer_set_info(this->sdk_equalizer_, this->rate_, this->num_channels_);
  for(int index=0; index<10; index++){
    this->set_eq(index, set_gain[index]);
  }

  // adf_pipeline-relatest setup
  sdk_audio_elements_.push_back(this->sdk_equalizer_);
  // this could lead to a conflict if there's more than one equalizer but will worry about it later
  sdk_element_tags_.push_back("equalizer");

  return true;
}

void ADFEqualizer::on_settings_request(AudioPipelineSettingsRequest &request){
  // Copied from ADFResampler impl
  bool settings_changed = false;
  if( request.sampling_rate > -1){
    if( request.sampling_rate != this->rate_ )
    {
      this->rate_ = request.sampling_rate;
      settings_changed = true;
    }
  }
  if( request.number_of_channels > -1){
    if( request.number_of_channels != this->num_channels_ )
    {
      this->num_channels_ = request.number_of_channels;
      settings_changed = true;
    }
  }

  if( this->sdk_equalizer_ && settings_changed)
  {
    if( audio_element_get_state(this->sdk_equalizer_) == AEL_STATE_RUNNING)
    {
      audio_element_stop(this->sdk_equalizer_);
      audio_element_wait_for_stop(this->sdk_equalizer_);
    }
    ADFPipelineElement* el = request.requested_by;
    if( el != nullptr ){
     esph_log_d(TAG, "Received request from: %s", el->get_name().c_str());
    }
    else {
     esph_log_d(TAG, "Called from invalid caller");
    }
    esph_log_d(TAG, "New equalizer settings: rate=%d, ch=%d.", this->rate_, this->num_channels_);
    equalizer_set_info(this->sdk_equalizer_, this->rate_, this->num_channels_);
  }

  esph_log_d(TAG, "Current settings: rate=%d, ch=%d.", this->rate_, this->num_channels_);
}

void ADFEqualizer::set_eq(int index, int value_gain){
  if (this->sdk_equalizer_) {
    // Based on the examples, it doesn't seem like we need to restart the pipeline to apply changes
    esph_log_d(TAG, "Adjusting equalizer for band=%d, to gain=%d.", index, value_gain);
    equalizer_set_gain_info(this->sdk_equalizer_, index, value_gain, true);
  }
}

}  // namespace esp_adf
}  // namespace esphome

#endif
