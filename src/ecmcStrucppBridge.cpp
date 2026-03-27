#include "ecmcStrucppBridge.hpp"

#include <cstring>
#include <sstream>

namespace {

ecmcStrucppIoImageSpan selectSpan(strucpp::LocatedArea area,
                                  const ecmcStrucppIoImages& images) {
  switch (area) {
  case strucpp::LocatedArea::Input:
    return images.input;
  case strucpp::LocatedArea::Output:
    return images.output;
  case strucpp::LocatedArea::Memory:
    return images.memory;
  }

  return {};
}

bool spanHasBytes(const ecmcStrucppIoImageSpan& span,
                  size_t byte_index,
                  size_t byte_count) {
  return span.data != nullptr && byte_index + byte_count <= span.size;
}

bool validateOneVar(const strucpp::LocatedVar& var,
                    const ecmcStrucppIoImages& images,
                    std::string* error_out) {
  const ecmcStrucppIoImageSpan span = selectSpan(var.area, images);
  const char area_char = strucpp::area_to_char(var.area);
  const char size_char = strucpp::size_to_char(var.size);

  if (!var.pointer) {
    if (error_out) {
      std::ostringstream oss;
      oss << "Located variable %" << area_char << size_char << var.byte_index;
      if (var.size == strucpp::LocatedSize::Bit) {
        oss << "." << static_cast<int>(var.bit_index);
      }
      oss << " is missing a storage pointer";
      *error_out = oss.str();
    }
    return false;
  }

  if (span.data == nullptr) {
    if (error_out) {
      std::ostringstream oss;
      oss << "No image configured for area %" << area_char;
      *error_out = oss.str();
    }
    return false;
  }

  if (var.size == strucpp::LocatedSize::Bit) {
    if (var.bit_index > 7 || var.byte_index >= span.size) {
      if (error_out) {
        std::ostringstream oss;
        oss << "Located bit %" << area_char << size_char << var.byte_index
            << "." << static_cast<int>(var.bit_index)
            << " is outside image '" << span.name << "' (" << span.size
            << " bytes)";
        *error_out = oss.str();
      }
      return false;
    }
    return true;
  }

  if (!spanHasBytes(span, var.byte_index, var.byte_size())) {
    if (error_out) {
      std::ostringstream oss;
      oss << "Located variable %" << area_char << size_char << var.byte_index
          << " (" << var.byte_size() << " bytes) is outside image '"
          << span.name << "' (" << span.size << " bytes)";
      *error_out = oss.str();
    }
    return false;
  }

  return true;
}

void appendCompiledOp(const strucpp::LocatedVar& var,
                      const ecmcStrucppIoImages& images,
                      ecmcStrucppCompiledCopyPlan* plan) {
  const ecmcStrucppIoImageSpan span = selectSpan(var.area, images);
  const uint8_t* const image_ptr = span.data + var.byte_index;

  if (var.size == strucpp::LocatedSize::Bit) {
    const uint8_t bit_mask = static_cast<uint8_t>(1U << var.bit_index);

    switch (var.area) {
    case strucpp::LocatedArea::Input:
      plan->input_bits_to_var.push_back(
        {image_ptr, bit_mask, static_cast<bool*>(var.pointer)});
      return;
    case strucpp::LocatedArea::Output:
      plan->output_bits_from_var.push_back({const_cast<uint8_t*>(image_ptr),
                                            bit_mask,
                                            static_cast<const bool*>(var.pointer)});
      return;
    case strucpp::LocatedArea::Memory:
      plan->memory_bits_to_var.push_back(
        {image_ptr, bit_mask, static_cast<bool*>(var.pointer)});
      plan->memory_bits_from_var.push_back({const_cast<uint8_t*>(image_ptr),
                                            bit_mask,
                                            static_cast<const bool*>(var.pointer)});
      return;
    }
  }

  const size_t byte_size = var.byte_size();
  switch (var.area) {
  case strucpp::LocatedArea::Input:
    plan->input_scalars_to_var.push_back({image_ptr, var.pointer, byte_size});
    return;
  case strucpp::LocatedArea::Output:
    plan->output_scalars_from_var.push_back(
      {const_cast<uint8_t*>(image_ptr), var.pointer, byte_size});
    return;
  case strucpp::LocatedArea::Memory:
    plan->memory_scalars_to_var.push_back({image_ptr, var.pointer, byte_size});
    plan->memory_scalars_from_var.push_back(
      {const_cast<uint8_t*>(image_ptr), var.pointer, byte_size});
    return;
  }
}

void executeScalarImageToVar(
  const std::vector<ecmcStrucppScalarImageToVarOp>& ops) {
  for (const auto& op : ops) {
    std::memcpy(op.var_ptr, op.image_ptr, op.size);
  }
}

void executeBitImageToVar(const std::vector<ecmcStrucppBitImageToVarOp>& ops) {
  for (const auto& op : ops) {
    *op.var_ptr = ((*op.image_byte & op.bit_mask) != 0U);
  }
}

void executeScalarVarToImage(
  const std::vector<ecmcStrucppScalarVarToImageOp>& ops) {
  for (const auto& op : ops) {
    std::memcpy(op.image_ptr, op.var_ptr, op.size);
  }
}

void executeBitVarToImage(const std::vector<ecmcStrucppBitVarToImageOp>& ops) {
  for (const auto& op : ops) {
    if (*op.var_ptr) {
      *op.image_byte = static_cast<uint8_t>(*op.image_byte | op.bit_mask);
    } else {
      *op.image_byte = static_cast<uint8_t>(*op.image_byte & ~op.bit_mask);
    }
  }
}

}  // namespace

bool ecmcStrucppValidateLocatedVars(const strucpp::LocatedVar* vars,
                                    size_t count,
                                    const ecmcStrucppIoImages& images,
                                    std::string* error_out) {
  if (!vars) {
    if (error_out) {
      *error_out = "Located variable table is null";
    }
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    if (!validateOneVar(vars[i], images, error_out)) {
      return false;
    }
  }

  return true;
}

bool ecmcStrucppBuildCopyPlan(const strucpp::LocatedVar* vars,
                              size_t count,
                              const ecmcStrucppIoImages& images,
                              ecmcStrucppCompiledCopyPlan* out_plan,
                              std::string* error_out) {
  if (!out_plan) {
    if (error_out) {
      *error_out = "Copy plan output pointer is null";
    }
    return false;
  }

  *out_plan = ecmcStrucppCompiledCopyPlan {};

  if (!ecmcStrucppValidateLocatedVars(vars, count, images, error_out)) {
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    appendCompiledOp(vars[i], images, out_plan);
  }

  return true;
}

void ecmcStrucppExecuteInputCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan) {
  executeBitImageToVar(plan.input_bits_to_var);
  executeScalarImageToVar(plan.input_scalars_to_var);
}

void ecmcStrucppExecuteMemoryPreCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan) {
  executeBitImageToVar(plan.memory_bits_to_var);
  executeScalarImageToVar(plan.memory_scalars_to_var);
}

void ecmcStrucppExecuteMemoryPostCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan) {
  executeBitVarToImage(plan.memory_bits_from_var);
  executeScalarVarToImage(plan.memory_scalars_from_var);
}

void ecmcStrucppExecuteOutputCopyPlan(
  const ecmcStrucppCompiledCopyPlan& plan) {
  executeBitVarToImage(plan.output_bits_from_var);
  executeScalarVarToImage(plan.output_scalars_from_var);
}
