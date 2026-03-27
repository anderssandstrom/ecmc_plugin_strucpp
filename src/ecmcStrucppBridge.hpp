#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "iec_located.hpp"

struct ecmcStrucppIoImageSpan {
  uint8_t* data {};
  size_t size {};
  std::string name;
};

struct ecmcStrucppIoImages {
  ecmcStrucppIoImageSpan input;
  ecmcStrucppIoImageSpan output;
  ecmcStrucppIoImageSpan memory;
};

struct ecmcStrucppScalarImageToVarOp {
  const uint8_t* image_ptr {};
  void* var_ptr {};
  size_t size {};
};

struct ecmcStrucppScalarVarToImageOp {
  uint8_t* image_ptr {};
  const void* var_ptr {};
  size_t size {};
};

struct ecmcStrucppBitImageToVarOp {
  const uint8_t* image_byte {};
  uint8_t bit_mask {};
  bool* var_ptr {};
};

struct ecmcStrucppBitVarToImageOp {
  uint8_t* image_byte {};
  uint8_t bit_mask {};
  const bool* var_ptr {};
};

struct ecmcStrucppCompiledCopyPlan {
  std::vector<ecmcStrucppScalarImageToVarOp> input_scalars_to_var;
  std::vector<ecmcStrucppBitImageToVarOp> input_bits_to_var;

  std::vector<ecmcStrucppScalarImageToVarOp> memory_scalars_to_var;
  std::vector<ecmcStrucppBitImageToVarOp> memory_bits_to_var;

  std::vector<ecmcStrucppScalarVarToImageOp> memory_scalars_from_var;
  std::vector<ecmcStrucppBitVarToImageOp> memory_bits_from_var;

  std::vector<ecmcStrucppScalarVarToImageOp> output_scalars_from_var;
  std::vector<ecmcStrucppBitVarToImageOp> output_bits_from_var;
};

bool ecmcStrucppValidateLocatedVars(const strucpp::LocatedVar* vars,
                                    size_t count,
                                    const ecmcStrucppIoImages& images,
                                    std::string* error_out);

bool ecmcStrucppBuildCopyPlan(const strucpp::LocatedVar* vars,
                              size_t count,
                              const ecmcStrucppIoImages& images,
                              ecmcStrucppCompiledCopyPlan* out_plan,
                              std::string* error_out);

void ecmcStrucppExecuteInputCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan);

void ecmcStrucppExecuteMemoryPreCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan);

void ecmcStrucppExecuteMemoryPostCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan);

void ecmcStrucppExecuteOutputCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan);
