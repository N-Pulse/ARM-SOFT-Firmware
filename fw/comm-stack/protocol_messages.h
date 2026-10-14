/*
 *  This file is generated with Embedded Proto, PLEASE DO NOT EDIT!
 *  source: protocol_messages.proto
 */

// This file is generated. Please do not edit!
#ifndef PROTOCOL_MESSAGES_H
#define PROTOCOL_MESSAGES_H

#include <cstdint>
#include <MessageInterface.h>
#include <WireFormatter.h>
#include <Fields.h>
#include <MessageSizeCalculator.h>
#include <ReadBufferSection.h>
#include <RepeatedFieldFixedSize.h>
#include <FieldStringBytes.h>
#include <Errors.h>
#include <Defines.h>
#include <limits>

// Include external proto definitions


class ClassificationData final: public ::EmbeddedProto::MessageInterface
{
  public:
    ClassificationData() = default;
    ClassificationData(const ClassificationData& rhs )
    {
      set_action(rhs.get_action());
    }

    ClassificationData(const ClassificationData&& rhs ) noexcept
    {
      set_action(rhs.get_action());
    }

    ~ClassificationData() override = default;

    enum class Action : uint32_t
    {
      OPEN_HAND = 0,
      CLOSE_HAND = 1,
      PINCH = 2,
      ROTATE_WRIST_R = 3,
      ROTATE_WRIST_L = 4
    };

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      ACTION = 1
    };

    ClassificationData& operator=(const ClassificationData& rhs)
    {
      set_action(rhs.get_action());
      return *this;
    }

    ClassificationData& operator=(const ClassificationData&& rhs) noexcept
    {
      set_action(rhs.get_action());
      return *this;
    }

    static constexpr char const* ACTION_NAME = "action";
    inline void clear_action() { action_.clear(); }
    inline void set_action(const Action& value) { action_ = value; }
    inline void set_action(const Action&& value) { action_ = value; }
    inline const Action& get_action() const { return action_.get(); }
    inline Action action() const { return action_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((static_cast<Action>(0) != action_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = action_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ACTION), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::ACTION:
            return_value = action_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_action();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::ACTION:
          name = ACTION_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = action_.to_string(left_chars, indent_level + 2, ACTION_NAME, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::enumeration<Action> action_ = static_cast<Action>(0);

};

template<
    uint32_t ClassificationParams_allowed_movements_REP_LENGTH
>
class ClassificationParams final: public ::EmbeddedProto::MessageInterface
{
  public:
    ClassificationParams() = default;
    ClassificationParams(const ClassificationParams& rhs )
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
      set_allowed_movements(rhs.get_allowed_movements());
      if(rhs.has_threshold())
      {
        set_threshold(rhs.get_threshold());
      }
      else
      {
        clear_threshold();
      }

    }

    ClassificationParams(const ClassificationParams&& rhs ) noexcept
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
      set_allowed_movements(rhs.get_allowed_movements());
      if(rhs.has_threshold())
      {
        set_threshold(rhs.get_threshold());
      }
      else
      {
        clear_threshold();
      }

    }

    ~ClassificationParams() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      DATA_RATE_HZ = 1,
      ALLOWED_MOVEMENTS = 2,
      THRESHOLD = 3
    };

    ClassificationParams& operator=(const ClassificationParams& rhs)
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
      set_allowed_movements(rhs.get_allowed_movements());
      if(rhs.has_threshold())
      {
        set_threshold(rhs.get_threshold());
      }
      else
      {
        clear_threshold();
      }

      return *this;
    }

    ClassificationParams& operator=(const ClassificationParams&& rhs) noexcept
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
      set_allowed_movements(rhs.get_allowed_movements());
      if(rhs.has_threshold())
      {
        set_threshold(rhs.get_threshold());
      }
      else
      {
        clear_threshold();
      }

      return *this;
    }

    static constexpr char const* DATA_RATE_HZ_NAME = "data_rate_hz";
    inline void clear_data_rate_hz() { data_rate_hz_.clear(); }
    inline void set_data_rate_hz(const uint32_t& value) { data_rate_hz_ = value; }
    inline void set_data_rate_hz(const uint32_t&& value) { data_rate_hz_ = value; }
    inline uint32_t& mutable_data_rate_hz() { return data_rate_hz_.get(); }
    inline const uint32_t& get_data_rate_hz() const { return data_rate_hz_.get(); }
    inline uint32_t data_rate_hz() const { return data_rate_hz_.get(); }

    static constexpr char const* ALLOWED_MOVEMENTS_NAME = "allowed_movements";
    inline const EmbeddedProto::uint32& allowed_movements(uint32_t index) const { return allowed_movements_[index]; }
    inline void clear_allowed_movements() { allowed_movements_.clear(); }
    inline void set_allowed_movements(uint32_t index, const EmbeddedProto::uint32& value) { allowed_movements_.set(index, value); }
    inline void set_allowed_movements(uint32_t index, const EmbeddedProto::uint32&& value) { allowed_movements_.set(index, value); }
    inline void set_allowed_movements(const ::EmbeddedProto::RepeatedFieldFixedSize<EmbeddedProto::uint32, ClassificationParams_allowed_movements_REP_LENGTH>& values) { allowed_movements_ = values; }
    inline void add_allowed_movements(const EmbeddedProto::uint32& value) { allowed_movements_.add(value); }
    inline ::EmbeddedProto::RepeatedFieldFixedSize<EmbeddedProto::uint32, ClassificationParams_allowed_movements_REP_LENGTH>& mutable_allowed_movements() { return allowed_movements_; }
    inline EmbeddedProto::uint32& mutable_allowed_movements(uint32_t index) { return allowed_movements_[index]; }
    inline const ::EmbeddedProto::RepeatedFieldFixedSize<EmbeddedProto::uint32, ClassificationParams_allowed_movements_REP_LENGTH>& get_allowed_movements() const { return allowed_movements_; }
    inline const ::EmbeddedProto::RepeatedFieldFixedSize<EmbeddedProto::uint32, ClassificationParams_allowed_movements_REP_LENGTH>& allowed_movements() const { return allowed_movements_; }

    static constexpr char const* THRESHOLD_NAME = "threshold";
    inline bool has_threshold() const
    {
      return 0 != (presence::mask(presence::fields::THRESHOLD) & presence_[presence::index(presence::fields::THRESHOLD)]);
    }
    inline void clear_threshold()
    {
      presence_[presence::index(presence::fields::THRESHOLD)] &= ~(presence::mask(presence::fields::THRESHOLD));
      threshold_.clear();
    }
    inline void set_threshold(const float& value)
    {
      presence_[presence::index(presence::fields::THRESHOLD)] |= presence::mask(presence::fields::THRESHOLD);
      threshold_ = value;
    }
    inline void set_threshold(const float&& value)
    {
      presence_[presence::index(presence::fields::THRESHOLD)] |= presence::mask(presence::fields::THRESHOLD);
      threshold_ = value;
    }
    inline float& mutable_threshold()
    {
      presence_[presence::index(presence::fields::THRESHOLD)] |= presence::mask(presence::fields::THRESHOLD);
      return threshold_.get();
    }
    inline const float& get_threshold() const { return threshold_.get(); }
    inline float threshold() const { return threshold_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != data_rate_hz_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = data_rate_hz_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DATA_RATE_HZ), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = allowed_movements_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ALLOWED_MOVEMENTS), buffer, false);
      }

      if(has_threshold() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = threshold_.serialize_with_id(static_cast<uint32_t>(FieldNumber::THRESHOLD), buffer, true);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::DATA_RATE_HZ:
            return_value = data_rate_hz_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ALLOWED_MOVEMENTS:
            return_value = allowed_movements_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::THRESHOLD:
            presence_[presence::index(presence::fields::THRESHOLD)] |= presence::mask(presence::fields::THRESHOLD);
            return_value = threshold_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_data_rate_hz();
      clear_allowed_movements();
      clear_threshold();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::DATA_RATE_HZ:
          name = DATA_RATE_HZ_NAME;
          break;
        case FieldNumber::ALLOWED_MOVEMENTS:
          name = ALLOWED_MOVEMENTS_NAME;
          break;
        case FieldNumber::THRESHOLD:
          name = THRESHOLD_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = data_rate_hz_.to_string(left_chars, indent_level + 2, DATA_RATE_HZ_NAME, true);
      left_chars = allowed_movements_.to_string(left_chars, indent_level + 2, ALLOWED_MOVEMENTS_NAME, false);
      left_chars = threshold_.to_string(left_chars, indent_level + 2, THRESHOLD_NAME, false);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:

      // Define constants for tracking the presence of fields.
      // Use a struct to scope the variables from user fields as namespaces are not allowed within classes.
      struct presence
      {
        // An enumeration with all the fields for which presence has to be tracked.
        enum class fields : uint32_t
        {
          THRESHOLD
        };

        // The number of fields for which presence has to be tracked.
        static constexpr uint32_t N_FIELDS = 1;

        // Which type are we using to track presence.
        using TYPE = uint32_t;

        // How many bits are there in the presence type.
        static constexpr uint32_t N_BITS = std::numeric_limits<TYPE>::digits;

        // How many variables of TYPE do we need to bit mask all presence fields.
        static constexpr uint32_t SIZE = (N_FIELDS / N_BITS) + ((N_FIELDS % N_BITS) > 0 ? 1 : 0);

        // Obtain the index of a given field in the presence array.
        static constexpr uint32_t index(const fields& field) { return static_cast<uint32_t>(field) / N_BITS; }

        // Obtain the bit mask for the given field assuming we are at the correct index in the presence array.
        static constexpr TYPE mask(const fields& field)
        {
          return static_cast<uint32_t>(0x01) << (static_cast<uint32_t>(field) % N_BITS);
        }
      };

      // Create an array in which the presence flags are stored.
      typename presence::TYPE presence_[presence::SIZE] = {0};

      EmbeddedProto::uint32 data_rate_hz_ = 0U;
      ::EmbeddedProto::RepeatedFieldFixedSize<EmbeddedProto::uint32, ClassificationParams_allowed_movements_REP_LENGTH> allowed_movements_;
      EmbeddedProto::floatfixed threshold_ = 0.0;

};

class CommandReply final: public ::EmbeddedProto::MessageInterface
{
  public:
    CommandReply() = default;
    CommandReply(const CommandReply& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_result(rhs.get_result());
      set_epoch(rhs.get_epoch());
      set_faults(rhs.get_faults());
    }

    CommandReply(const CommandReply&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_result(rhs.get_result());
      set_epoch(rhs.get_epoch());
      set_faults(rhs.get_faults());
    }

    ~CommandReply() override = default;

    enum class Result : uint32_t
    {
      OK = 0,
      PRODUCTION = 1,
      SESSION = 2,
      STALE = 3,
      WRONG_MOTOR = 4,
      PARAMETERS = 5,
      DISABLE_FIRST = 6,
      CALIBRATION = 7,
      FEEDBACK = 8,
      FAULT = 9,
      ARM_FIRST = 10,
      ENABLE_FIRST = 11,
      REJECTED = 12
    };

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      RESULT = 2,
      EPOCH = 3,
      FAULTS = 4
    };

    CommandReply& operator=(const CommandReply& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_result(rhs.get_result());
      set_epoch(rhs.get_epoch());
      set_faults(rhs.get_faults());
      return *this;
    }

    CommandReply& operator=(const CommandReply&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_result(rhs.get_result());
      set_epoch(rhs.get_epoch());
      set_faults(rhs.get_faults());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* RESULT_NAME = "result";
    inline void clear_result() { result_.clear(); }
    inline void set_result(const Result& value) { result_ = value; }
    inline void set_result(const Result&& value) { result_ = value; }
    inline const Result& get_result() const { return result_.get(); }
    inline Result result() const { return result_.get(); }

    static constexpr char const* EPOCH_NAME = "epoch";
    inline void clear_epoch() { epoch_.clear(); }
    inline void set_epoch(const uint32_t& value) { epoch_ = value; }
    inline void set_epoch(const uint32_t&& value) { epoch_ = value; }
    inline uint32_t& mutable_epoch() { return epoch_.get(); }
    inline const uint32_t& get_epoch() const { return epoch_.get(); }
    inline uint32_t epoch() const { return epoch_.get(); }

    static constexpr char const* FAULTS_NAME = "faults";
    inline void clear_faults() { faults_.clear(); }
    inline void set_faults(const uint32_t& value) { faults_ = value; }
    inline void set_faults(const uint32_t&& value) { faults_ = value; }
    inline uint32_t& mutable_faults() { return faults_.get(); }
    inline const uint32_t& get_faults() const { return faults_.get(); }
    inline uint32_t faults() const { return faults_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((static_cast<Result>(0) != result_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = result_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RESULT), buffer, false);
      }

      if((0U != epoch_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = epoch_.serialize_with_id(static_cast<uint32_t>(FieldNumber::EPOCH), buffer, false);
      }

      if((0U != faults_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = faults_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FAULTS), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::REQUEST_ID:
            return_value = request_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RESULT:
            return_value = result_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::EPOCH:
            return_value = epoch_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FAULTS:
            return_value = faults_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_request_id();
      clear_result();
      clear_epoch();
      clear_faults();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::REQUEST_ID:
          name = REQUEST_ID_NAME;
          break;
        case FieldNumber::RESULT:
          name = RESULT_NAME;
          break;
        case FieldNumber::EPOCH:
          name = EPOCH_NAME;
          break;
        case FieldNumber::FAULTS:
          name = FAULTS_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = request_id_.to_string(left_chars, indent_level + 2, REQUEST_ID_NAME, true);
      left_chars = result_.to_string(left_chars, indent_level + 2, RESULT_NAME, false);
      left_chars = epoch_.to_string(left_chars, indent_level + 2, EPOCH_NAME, false);
      left_chars = faults_.to_string(left_chars, indent_level + 2, FAULTS_NAME, false);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 request_id_ = 0U;
      EmbeddedProto::enumeration<Result> result_ = static_cast<Result>(0);
      EmbeddedProto::uint32 epoch_ = 0U;
      EmbeddedProto::uint32 faults_ = 0U;

};

class CommissionCommand final: public ::EmbeddedProto::MessageInterface
{
  public:
    CommissionCommand() = default;
    CommissionCommand(const CommissionCommand& rhs )
    {
      set_request_id(rhs.get_request_id());
      set_epoch(rhs.get_epoch());
      set_operation(rhs.get_operation());
      set_motor_id(rhs.get_motor_id());
      set_cpr(rhs.get_cpr());
      set_quadrature(rhs.get_quadrature());
      set_gear_ratio(rhs.get_gear_ratio());
      set_direction(rhs.get_direction());
      set_zero_offset(rhs.get_zero_offset());
      set_minimum_rad(rhs.get_minimum_rad());
      set_maximum_rad(rhs.get_maximum_rad());
      set_maximum_step_rad(rhs.get_maximum_step_rad());
      set_pwm_percent(rhs.get_pwm_percent());
      set_target_rad(rhs.get_target_rad());
      set_reference_rate_rad_s(rhs.get_reference_rate_rad_s());
      set_stationary_reference_verified(rhs.get_stationary_reference_verified());
    }

    CommissionCommand(const CommissionCommand&& rhs ) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_epoch(rhs.get_epoch());
      set_operation(rhs.get_operation());
      set_motor_id(rhs.get_motor_id());
      set_cpr(rhs.get_cpr());
      set_quadrature(rhs.get_quadrature());
      set_gear_ratio(rhs.get_gear_ratio());
      set_direction(rhs.get_direction());
      set_zero_offset(rhs.get_zero_offset());
      set_minimum_rad(rhs.get_minimum_rad());
      set_maximum_rad(rhs.get_maximum_rad());
      set_maximum_step_rad(rhs.get_maximum_step_rad());
      set_pwm_percent(rhs.get_pwm_percent());
      set_target_rad(rhs.get_target_rad());
      set_reference_rate_rad_s(rhs.get_reference_rate_rad_s());
      set_stationary_reference_verified(rhs.get_stationary_reference_verified());
    }

    ~CommissionCommand() override = default;

    enum class Operation : uint32_t
    {
      UNSPECIFIED = 0,
      LIMITS = 1,
      CALIBRATE = 2,
      ZERO = 3,
      RECOVER = 4,
      ARM = 5,
      ENABLE = 6,
      TARGET = 7,
      DISABLE = 8,
      ESTIMATOR_RESET = 9,
      ESTIMATOR_STOP = 10,
      OUTPUT_PREFLIGHT = 11,
      LINK_AUTHORIZE = 12,
      LINK_RECOVER = 13
    };

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REQUEST_ID = 1,
      EPOCH = 2,
      OPERATION = 3,
      MOTOR_ID = 4,
      CPR = 5,
      QUADRATURE = 6,
      GEAR_RATIO = 7,
      DIRECTION = 8,
      ZERO_OFFSET = 9,
      MINIMUM_RAD = 10,
      MAXIMUM_RAD = 11,
      MAXIMUM_STEP_RAD = 12,
      PWM_PERCENT = 13,
      TARGET_RAD = 14,
      REFERENCE_RATE_RAD_S = 15,
      STATIONARY_REFERENCE_VERIFIED = 16
    };

    CommissionCommand& operator=(const CommissionCommand& rhs)
    {
      set_request_id(rhs.get_request_id());
      set_epoch(rhs.get_epoch());
      set_operation(rhs.get_operation());
      set_motor_id(rhs.get_motor_id());
      set_cpr(rhs.get_cpr());
      set_quadrature(rhs.get_quadrature());
      set_gear_ratio(rhs.get_gear_ratio());
      set_direction(rhs.get_direction());
      set_zero_offset(rhs.get_zero_offset());
      set_minimum_rad(rhs.get_minimum_rad());
      set_maximum_rad(rhs.get_maximum_rad());
      set_maximum_step_rad(rhs.get_maximum_step_rad());
      set_pwm_percent(rhs.get_pwm_percent());
      set_target_rad(rhs.get_target_rad());
      set_reference_rate_rad_s(rhs.get_reference_rate_rad_s());
      set_stationary_reference_verified(rhs.get_stationary_reference_verified());
      return *this;
    }

    CommissionCommand& operator=(const CommissionCommand&& rhs) noexcept
    {
      set_request_id(rhs.get_request_id());
      set_epoch(rhs.get_epoch());
      set_operation(rhs.get_operation());
      set_motor_id(rhs.get_motor_id());
      set_cpr(rhs.get_cpr());
      set_quadrature(rhs.get_quadrature());
      set_gear_ratio(rhs.get_gear_ratio());
      set_direction(rhs.get_direction());
      set_zero_offset(rhs.get_zero_offset());
      set_minimum_rad(rhs.get_minimum_rad());
      set_maximum_rad(rhs.get_maximum_rad());
      set_maximum_step_rad(rhs.get_maximum_step_rad());
      set_pwm_percent(rhs.get_pwm_percent());
      set_target_rad(rhs.get_target_rad());
      set_reference_rate_rad_s(rhs.get_reference_rate_rad_s());
      set_stationary_reference_verified(rhs.get_stationary_reference_verified());
      return *this;
    }

    static constexpr char const* REQUEST_ID_NAME = "request_id";
    inline void clear_request_id() { request_id_.clear(); }
    inline void set_request_id(const uint32_t& value) { request_id_ = value; }
    inline void set_request_id(const uint32_t&& value) { request_id_ = value; }
    inline uint32_t& mutable_request_id() { return request_id_.get(); }
    inline const uint32_t& get_request_id() const { return request_id_.get(); }
    inline uint32_t request_id() const { return request_id_.get(); }

    static constexpr char const* EPOCH_NAME = "epoch";
    inline void clear_epoch() { epoch_.clear(); }
    inline void set_epoch(const uint32_t& value) { epoch_ = value; }
    inline void set_epoch(const uint32_t&& value) { epoch_ = value; }
    inline uint32_t& mutable_epoch() { return epoch_.get(); }
    inline const uint32_t& get_epoch() const { return epoch_.get(); }
    inline uint32_t epoch() const { return epoch_.get(); }

    static constexpr char const* OPERATION_NAME = "operation";
    inline void clear_operation() { operation_.clear(); }
    inline void set_operation(const Operation& value) { operation_ = value; }
    inline void set_operation(const Operation&& value) { operation_ = value; }
    inline const Operation& get_operation() const { return operation_.get(); }
    inline Operation operation() const { return operation_.get(); }

    static constexpr char const* MOTOR_ID_NAME = "motor_id";
    inline void clear_motor_id() { motor_id_.clear(); }
    inline void set_motor_id(const uint32_t& value) { motor_id_ = value; }
    inline void set_motor_id(const uint32_t&& value) { motor_id_ = value; }
    inline uint32_t& mutable_motor_id() { return motor_id_.get(); }
    inline const uint32_t& get_motor_id() const { return motor_id_.get(); }
    inline uint32_t motor_id() const { return motor_id_.get(); }

    static constexpr char const* CPR_NAME = "cpr";
    inline void clear_cpr() { cpr_.clear(); }
    inline void set_cpr(const uint32_t& value) { cpr_ = value; }
    inline void set_cpr(const uint32_t&& value) { cpr_ = value; }
    inline uint32_t& mutable_cpr() { return cpr_.get(); }
    inline const uint32_t& get_cpr() const { return cpr_.get(); }
    inline uint32_t cpr() const { return cpr_.get(); }

    static constexpr char const* QUADRATURE_NAME = "quadrature";
    inline void clear_quadrature() { quadrature_.clear(); }
    inline void set_quadrature(const uint32_t& value) { quadrature_ = value; }
    inline void set_quadrature(const uint32_t&& value) { quadrature_ = value; }
    inline uint32_t& mutable_quadrature() { return quadrature_.get(); }
    inline const uint32_t& get_quadrature() const { return quadrature_.get(); }
    inline uint32_t quadrature() const { return quadrature_.get(); }

    static constexpr char const* GEAR_RATIO_NAME = "gear_ratio";
    inline void clear_gear_ratio() { gear_ratio_.clear(); }
    inline void set_gear_ratio(const float& value) { gear_ratio_ = value; }
    inline void set_gear_ratio(const float&& value) { gear_ratio_ = value; }
    inline float& mutable_gear_ratio() { return gear_ratio_.get(); }
    inline const float& get_gear_ratio() const { return gear_ratio_.get(); }
    inline float gear_ratio() const { return gear_ratio_.get(); }

    static constexpr char const* DIRECTION_NAME = "direction";
    inline void clear_direction() { direction_.clear(); }
    inline void set_direction(const int32_t& value) { direction_ = value; }
    inline void set_direction(const int32_t&& value) { direction_ = value; }
    inline int32_t& mutable_direction() { return direction_.get(); }
    inline const int32_t& get_direction() const { return direction_.get(); }
    inline int32_t direction() const { return direction_.get(); }

    static constexpr char const* ZERO_OFFSET_NAME = "zero_offset";
    inline void clear_zero_offset() { zero_offset_.clear(); }
    inline void set_zero_offset(const int64_t& value) { zero_offset_ = value; }
    inline void set_zero_offset(const int64_t&& value) { zero_offset_ = value; }
    inline int64_t& mutable_zero_offset() { return zero_offset_.get(); }
    inline const int64_t& get_zero_offset() const { return zero_offset_.get(); }
    inline int64_t zero_offset() const { return zero_offset_.get(); }

    static constexpr char const* MINIMUM_RAD_NAME = "minimum_rad";
    inline void clear_minimum_rad() { minimum_rad_.clear(); }
    inline void set_minimum_rad(const float& value) { minimum_rad_ = value; }
    inline void set_minimum_rad(const float&& value) { minimum_rad_ = value; }
    inline float& mutable_minimum_rad() { return minimum_rad_.get(); }
    inline const float& get_minimum_rad() const { return minimum_rad_.get(); }
    inline float minimum_rad() const { return minimum_rad_.get(); }

    static constexpr char const* MAXIMUM_RAD_NAME = "maximum_rad";
    inline void clear_maximum_rad() { maximum_rad_.clear(); }
    inline void set_maximum_rad(const float& value) { maximum_rad_ = value; }
    inline void set_maximum_rad(const float&& value) { maximum_rad_ = value; }
    inline float& mutable_maximum_rad() { return maximum_rad_.get(); }
    inline const float& get_maximum_rad() const { return maximum_rad_.get(); }
    inline float maximum_rad() const { return maximum_rad_.get(); }

    static constexpr char const* MAXIMUM_STEP_RAD_NAME = "maximum_step_rad";
    inline void clear_maximum_step_rad() { maximum_step_rad_.clear(); }
    inline void set_maximum_step_rad(const float& value) { maximum_step_rad_ = value; }
    inline void set_maximum_step_rad(const float&& value) { maximum_step_rad_ = value; }
    inline float& mutable_maximum_step_rad() { return maximum_step_rad_.get(); }
    inline const float& get_maximum_step_rad() const { return maximum_step_rad_.get(); }
    inline float maximum_step_rad() const { return maximum_step_rad_.get(); }

    static constexpr char const* PWM_PERCENT_NAME = "pwm_percent";
    inline void clear_pwm_percent() { pwm_percent_.clear(); }
    inline void set_pwm_percent(const uint32_t& value) { pwm_percent_ = value; }
    inline void set_pwm_percent(const uint32_t&& value) { pwm_percent_ = value; }
    inline uint32_t& mutable_pwm_percent() { return pwm_percent_.get(); }
    inline const uint32_t& get_pwm_percent() const { return pwm_percent_.get(); }
    inline uint32_t pwm_percent() const { return pwm_percent_.get(); }

    static constexpr char const* TARGET_RAD_NAME = "target_rad";
    inline void clear_target_rad() { target_rad_.clear(); }
    inline void set_target_rad(const float& value) { target_rad_ = value; }
    inline void set_target_rad(const float&& value) { target_rad_ = value; }
    inline float& mutable_target_rad() { return target_rad_.get(); }
    inline const float& get_target_rad() const { return target_rad_.get(); }
    inline float target_rad() const { return target_rad_.get(); }

    static constexpr char const* REFERENCE_RATE_RAD_S_NAME = "reference_rate_rad_s";
    inline void clear_reference_rate_rad_s() { reference_rate_rad_s_.clear(); }
    inline void set_reference_rate_rad_s(const float& value) { reference_rate_rad_s_ = value; }
    inline void set_reference_rate_rad_s(const float&& value) { reference_rate_rad_s_ = value; }
    inline float& mutable_reference_rate_rad_s() { return reference_rate_rad_s_.get(); }
    inline const float& get_reference_rate_rad_s() const { return reference_rate_rad_s_.get(); }
    inline float reference_rate_rad_s() const { return reference_rate_rad_s_.get(); }

    static constexpr char const* STATIONARY_REFERENCE_VERIFIED_NAME = "stationary_reference_verified";
    inline void clear_stationary_reference_verified() { stationary_reference_verified_.clear(); }
    inline void set_stationary_reference_verified(const bool& value) { stationary_reference_verified_ = value; }
    inline void set_stationary_reference_verified(const bool&& value) { stationary_reference_verified_ = value; }
    inline bool& mutable_stationary_reference_verified() { return stationary_reference_verified_.get(); }
    inline const bool& get_stationary_reference_verified() const { return stationary_reference_verified_.get(); }
    inline bool stationary_reference_verified() const { return stationary_reference_verified_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != request_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = request_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REQUEST_ID), buffer, false);
      }

      if((0U != epoch_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = epoch_.serialize_with_id(static_cast<uint32_t>(FieldNumber::EPOCH), buffer, false);
      }

      if((static_cast<Operation>(0) != operation_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = operation_.serialize_with_id(static_cast<uint32_t>(FieldNumber::OPERATION), buffer, false);
      }

      if((0U != motor_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = motor_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MOTOR_ID), buffer, false);
      }

      if((0U != cpr_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = cpr_.serialize_with_id(static_cast<uint32_t>(FieldNumber::CPR), buffer, false);
      }

      if((0U != quadrature_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = quadrature_.serialize_with_id(static_cast<uint32_t>(FieldNumber::QUADRATURE), buffer, false);
      }

      if((0.0 != gear_ratio_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = gear_ratio_.serialize_with_id(static_cast<uint32_t>(FieldNumber::GEAR_RATIO), buffer, false);
      }

      if((0 != direction_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = direction_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DIRECTION), buffer, false);
      }

      if((0 != zero_offset_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = zero_offset_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ZERO_OFFSET), buffer, false);
      }

      if((0.0 != minimum_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = minimum_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MINIMUM_RAD), buffer, false);
      }

      if((0.0 != maximum_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = maximum_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MAXIMUM_RAD), buffer, false);
      }

      if((0.0 != maximum_step_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = maximum_step_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MAXIMUM_STEP_RAD), buffer, false);
      }

      if((0U != pwm_percent_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = pwm_percent_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PWM_PERCENT), buffer, false);
      }

      if((0.0 != target_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = target_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TARGET_RAD), buffer, false);
      }

      if((0.0 != reference_rate_rad_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = reference_rate_rad_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REFERENCE_RATE_RAD_S), buffer, false);
      }

      if((false != stationary_reference_verified_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = stationary_reference_verified_.serialize_with_id(static_cast<uint32_t>(FieldNumber::STATIONARY_REFERENCE_VERIFIED), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::REQUEST_ID:
            return_value = request_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::EPOCH:
            return_value = epoch_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::OPERATION:
            return_value = operation_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MOTOR_ID:
            return_value = motor_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::CPR:
            return_value = cpr_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::QUADRATURE:
            return_value = quadrature_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::GEAR_RATIO:
            return_value = gear_ratio_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::DIRECTION:
            return_value = direction_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ZERO_OFFSET:
            return_value = zero_offset_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MINIMUM_RAD:
            return_value = minimum_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MAXIMUM_RAD:
            return_value = maximum_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MAXIMUM_STEP_RAD:
            return_value = maximum_step_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PWM_PERCENT:
            return_value = pwm_percent_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TARGET_RAD:
            return_value = target_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::REFERENCE_RATE_RAD_S:
            return_value = reference_rate_rad_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::STATIONARY_REFERENCE_VERIFIED:
            return_value = stationary_reference_verified_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_request_id();
      clear_epoch();
      clear_operation();
      clear_motor_id();
      clear_cpr();
      clear_quadrature();
      clear_gear_ratio();
      clear_direction();
      clear_zero_offset();
      clear_minimum_rad();
      clear_maximum_rad();
      clear_maximum_step_rad();
      clear_pwm_percent();
      clear_target_rad();
      clear_reference_rate_rad_s();
      clear_stationary_reference_verified();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::REQUEST_ID:
          name = REQUEST_ID_NAME;
          break;
        case FieldNumber::EPOCH:
          name = EPOCH_NAME;
          break;
        case FieldNumber::OPERATION:
          name = OPERATION_NAME;
          break;
        case FieldNumber::MOTOR_ID:
          name = MOTOR_ID_NAME;
          break;
        case FieldNumber::CPR:
          name = CPR_NAME;
          break;
        case FieldNumber::QUADRATURE:
          name = QUADRATURE_NAME;
          break;
        case FieldNumber::GEAR_RATIO:
          name = GEAR_RATIO_NAME;
          break;
        case FieldNumber::DIRECTION:
          name = DIRECTION_NAME;
          break;
        case FieldNumber::ZERO_OFFSET:
          name = ZERO_OFFSET_NAME;
          break;
        case FieldNumber::MINIMUM_RAD:
          name = MINIMUM_RAD_NAME;
          break;
        case FieldNumber::MAXIMUM_RAD:
          name = MAXIMUM_RAD_NAME;
          break;
        case FieldNumber::MAXIMUM_STEP_RAD:
          name = MAXIMUM_STEP_RAD_NAME;
          break;
        case FieldNumber::PWM_PERCENT:
          name = PWM_PERCENT_NAME;
          break;
        case FieldNumber::TARGET_RAD:
          name = TARGET_RAD_NAME;
          break;
        case FieldNumber::REFERENCE_RATE_RAD_S:
          name = REFERENCE_RATE_RAD_S_NAME;
          break;
        case FieldNumber::STATIONARY_REFERENCE_VERIFIED:
          name = STATIONARY_REFERENCE_VERIFIED_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = request_id_.to_string(left_chars, indent_level + 2, REQUEST_ID_NAME, true);
      left_chars = epoch_.to_string(left_chars, indent_level + 2, EPOCH_NAME, false);
      left_chars = operation_.to_string(left_chars, indent_level + 2, OPERATION_NAME, false);
      left_chars = motor_id_.to_string(left_chars, indent_level + 2, MOTOR_ID_NAME, false);
      left_chars = cpr_.to_string(left_chars, indent_level + 2, CPR_NAME, false);
      left_chars = quadrature_.to_string(left_chars, indent_level + 2, QUADRATURE_NAME, false);
      left_chars = gear_ratio_.to_string(left_chars, indent_level + 2, GEAR_RATIO_NAME, false);
      left_chars = direction_.to_string(left_chars, indent_level + 2, DIRECTION_NAME, false);
      left_chars = zero_offset_.to_string(left_chars, indent_level + 2, ZERO_OFFSET_NAME, false);
      left_chars = minimum_rad_.to_string(left_chars, indent_level + 2, MINIMUM_RAD_NAME, false);
      left_chars = maximum_rad_.to_string(left_chars, indent_level + 2, MAXIMUM_RAD_NAME, false);
      left_chars = maximum_step_rad_.to_string(left_chars, indent_level + 2, MAXIMUM_STEP_RAD_NAME, false);
      left_chars = pwm_percent_.to_string(left_chars, indent_level + 2, PWM_PERCENT_NAME, false);
      left_chars = target_rad_.to_string(left_chars, indent_level + 2, TARGET_RAD_NAME, false);
      left_chars = reference_rate_rad_s_.to_string(left_chars, indent_level + 2, REFERENCE_RATE_RAD_S_NAME, false);
      left_chars = stationary_reference_verified_.to_string(left_chars, indent_level + 2, STATIONARY_REFERENCE_VERIFIED_NAME, false);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 request_id_ = 0U;
      EmbeddedProto::uint32 epoch_ = 0U;
      EmbeddedProto::enumeration<Operation> operation_ = static_cast<Operation>(0);
      EmbeddedProto::uint32 motor_id_ = 0U;
      EmbeddedProto::uint32 cpr_ = 0U;
      EmbeddedProto::uint32 quadrature_ = 0U;
      EmbeddedProto::floatfixed gear_ratio_ = 0.0;
      EmbeddedProto::sint32 direction_ = 0;
      EmbeddedProto::sint64 zero_offset_ = 0;
      EmbeddedProto::floatfixed minimum_rad_ = 0.0;
      EmbeddedProto::floatfixed maximum_rad_ = 0.0;
      EmbeddedProto::floatfixed maximum_step_rad_ = 0.0;
      EmbeddedProto::uint32 pwm_percent_ = 0U;
      EmbeddedProto::floatfixed target_rad_ = 0.0;
      EmbeddedProto::floatfixed reference_rate_rad_s_ = 0.0;
      EmbeddedProto::boolean stationary_reference_verified_ = false;

};

class DiagnosticRequest final: public ::EmbeddedProto::MessageInterface
{
  public:
    DiagnosticRequest() = default;
    DiagnosticRequest(const DiagnosticRequest& rhs )
    {
      set_motor_id(rhs.get_motor_id());
    }

    DiagnosticRequest(const DiagnosticRequest&& rhs ) noexcept
    {
      set_motor_id(rhs.get_motor_id());
    }

    ~DiagnosticRequest() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      MOTOR_ID = 1
    };

    DiagnosticRequest& operator=(const DiagnosticRequest& rhs)
    {
      set_motor_id(rhs.get_motor_id());
      return *this;
    }

    DiagnosticRequest& operator=(const DiagnosticRequest&& rhs) noexcept
    {
      set_motor_id(rhs.get_motor_id());
      return *this;
    }

    static constexpr char const* MOTOR_ID_NAME = "motor_id";
    inline void clear_motor_id() { motor_id_.clear(); }
    inline void set_motor_id(const uint32_t& value) { motor_id_ = value; }
    inline void set_motor_id(const uint32_t&& value) { motor_id_ = value; }
    inline uint32_t& mutable_motor_id() { return motor_id_.get(); }
    inline const uint32_t& get_motor_id() const { return motor_id_.get(); }
    inline uint32_t motor_id() const { return motor_id_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != motor_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = motor_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MOTOR_ID), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::MOTOR_ID:
            return_value = motor_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_motor_id();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::MOTOR_ID:
          name = MOTOR_ID_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = motor_id_.to_string(left_chars, indent_level + 2, MOTOR_ID_NAME, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 motor_id_ = 0U;

};

class Error final: public ::EmbeddedProto::MessageInterface
{
  public:
    Error() = default;
    Error(const Error& rhs )
    {
      set_reason(rhs.get_reason());
    }

    Error(const Error&& rhs ) noexcept
    {
      set_reason(rhs.get_reason());
    }

    ~Error() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      REASON = 1
    };

    Error& operator=(const Error& rhs)
    {
      set_reason(rhs.get_reason());
      return *this;
    }

    Error& operator=(const Error&& rhs) noexcept
    {
      set_reason(rhs.get_reason());
      return *this;
    }

    static constexpr char const* REASON_NAME = "reason";
    inline void clear_reason() { reason_.clear(); }
    inline void set_reason(const uint32_t& value) { reason_ = value; }
    inline void set_reason(const uint32_t&& value) { reason_ = value; }
    inline uint32_t& mutable_reason() { return reason_.get(); }
    inline const uint32_t& get_reason() const { return reason_.get(); }
    inline uint32_t reason() const { return reason_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != reason_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = reason_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REASON), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::REASON:
            return_value = reason_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_reason();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::REASON:
          name = REASON_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = reason_.to_string(left_chars, indent_level + 2, REASON_NAME, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 reason_ = 0U;

};

class Hello final: public ::EmbeddedProto::MessageInterface
{
  public:
    Hello() = default;
    Hello(const Hello& rhs )
    {
      set_device_id(rhs.get_device_id());
      set_proto_ver(rhs.get_proto_ver());
      set_supported_modes(rhs.get_supported_modes());
    }

    Hello(const Hello&& rhs ) noexcept
    {
      set_device_id(rhs.get_device_id());
      set_proto_ver(rhs.get_proto_ver());
      set_supported_modes(rhs.get_supported_modes());
    }

    ~Hello() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      DEVICE_ID = 1,
      PROTO_VER = 2,
      SUPPORTED_MODES = 3
    };

    Hello& operator=(const Hello& rhs)
    {
      set_device_id(rhs.get_device_id());
      set_proto_ver(rhs.get_proto_ver());
      set_supported_modes(rhs.get_supported_modes());
      return *this;
    }

    Hello& operator=(const Hello&& rhs) noexcept
    {
      set_device_id(rhs.get_device_id());
      set_proto_ver(rhs.get_proto_ver());
      set_supported_modes(rhs.get_supported_modes());
      return *this;
    }

    static constexpr char const* DEVICE_ID_NAME = "device_id";
    inline void clear_device_id() { device_id_.clear(); }
    inline void set_device_id(const uint32_t& value) { device_id_ = value; }
    inline void set_device_id(const uint32_t&& value) { device_id_ = value; }
    inline uint32_t& mutable_device_id() { return device_id_.get(); }
    inline const uint32_t& get_device_id() const { return device_id_.get(); }
    inline uint32_t device_id() const { return device_id_.get(); }

    static constexpr char const* PROTO_VER_NAME = "proto_ver";
    inline void clear_proto_ver() { proto_ver_.clear(); }
    inline void set_proto_ver(const uint32_t& value) { proto_ver_ = value; }
    inline void set_proto_ver(const uint32_t&& value) { proto_ver_ = value; }
    inline uint32_t& mutable_proto_ver() { return proto_ver_.get(); }
    inline const uint32_t& get_proto_ver() const { return proto_ver_.get(); }
    inline uint32_t proto_ver() const { return proto_ver_.get(); }

    static constexpr char const* SUPPORTED_MODES_NAME = "supported_modes";
    inline void clear_supported_modes() { supported_modes_.clear(); }
    inline void set_supported_modes(const uint32_t& value) { supported_modes_ = value; }
    inline void set_supported_modes(const uint32_t&& value) { supported_modes_ = value; }
    inline uint32_t& mutable_supported_modes() { return supported_modes_.get(); }
    inline const uint32_t& get_supported_modes() const { return supported_modes_.get(); }
    inline uint32_t supported_modes() const { return supported_modes_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != device_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = device_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DEVICE_ID), buffer, false);
      }

      if((0U != proto_ver_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = proto_ver_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PROTO_VER), buffer, false);
      }

      if((0U != supported_modes_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = supported_modes_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SUPPORTED_MODES), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::DEVICE_ID:
            return_value = device_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PROTO_VER:
            return_value = proto_ver_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::SUPPORTED_MODES:
            return_value = supported_modes_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_device_id();
      clear_proto_ver();
      clear_supported_modes();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::DEVICE_ID:
          name = DEVICE_ID_NAME;
          break;
        case FieldNumber::PROTO_VER:
          name = PROTO_VER_NAME;
          break;
        case FieldNumber::SUPPORTED_MODES:
          name = SUPPORTED_MODES_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = device_id_.to_string(left_chars, indent_level + 2, DEVICE_ID_NAME, true);
      left_chars = proto_ver_.to_string(left_chars, indent_level + 2, PROTO_VER_NAME, false);
      left_chars = supported_modes_.to_string(left_chars, indent_level + 2, SUPPORTED_MODES_NAME, false);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 device_id_ = 0U;
      EmbeddedProto::uint32 proto_ver_ = 0U;
      EmbeddedProto::uint32 supported_modes_ = 0U;

};

class LogisticRegressionParams final: public ::EmbeddedProto::MessageInterface
{
  public:
    LogisticRegressionParams() = default;
    LogisticRegressionParams(const LogisticRegressionParams& rhs )
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
    }

    LogisticRegressionParams(const LogisticRegressionParams&& rhs ) noexcept
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
    }

    ~LogisticRegressionParams() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      DATA_RATE_HZ = 1
    };

    LogisticRegressionParams& operator=(const LogisticRegressionParams& rhs)
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
      return *this;
    }

    LogisticRegressionParams& operator=(const LogisticRegressionParams&& rhs) noexcept
    {
      set_data_rate_hz(rhs.get_data_rate_hz());
      return *this;
    }

    static constexpr char const* DATA_RATE_HZ_NAME = "data_rate_hz";
    inline void clear_data_rate_hz() { data_rate_hz_.clear(); }
    inline void set_data_rate_hz(const uint32_t& value) { data_rate_hz_ = value; }
    inline void set_data_rate_hz(const uint32_t&& value) { data_rate_hz_ = value; }
    inline uint32_t& mutable_data_rate_hz() { return data_rate_hz_.get(); }
    inline const uint32_t& get_data_rate_hz() const { return data_rate_hz_.get(); }
    inline uint32_t data_rate_hz() const { return data_rate_hz_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != data_rate_hz_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = data_rate_hz_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DATA_RATE_HZ), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::DATA_RATE_HZ:
            return_value = data_rate_hz_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_data_rate_hz();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::DATA_RATE_HZ:
          name = DATA_RATE_HZ_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = data_rate_hz_.to_string(left_chars, indent_level + 2, DATA_RATE_HZ_NAME, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 data_rate_hz_ = 0U;

};

class MotorDiagnostic final: public ::EmbeddedProto::MessageInterface
{
  public:
    MotorDiagnostic() = default;
    MotorDiagnostic(const MotorDiagnostic& rhs )
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_motor_id(rhs.get_motor_id());
      set_epoch(rhs.get_epoch());
      set_flags(rhs.get_flags());
      set_faults(rhs.get_faults());
      set_target_rad(rhs.get_target_rad());
      set_reference_rad(rhs.get_reference_rad());
      set_raw_count(rhs.get_raw_count());
      set_encoder_rad(rhs.get_encoder_rad());
      set_velocity_rad_s(rhs.get_velocity_rad_s());
      set_pwm_percent(rhs.get_pwm_percent());
      set_adc_raw(rhs.get_adc_raw());
      set_adc_voltage(rhs.get_adc_voltage());
      set_current_a(rhs.get_current_a());
      set_adc_state(rhs.get_adc_state());
      set_adc_error(rhs.get_adc_error());
      set_adc_age_ms(rhs.get_adc_age_ms());
      set_kalman_rad(rhs.get_kalman_rad());
      set_kalman_velocity_rad_s(rhs.get_kalman_velocity_rad_s());
      set_kalman_status(rhs.get_kalman_status());
    }

    MotorDiagnostic(const MotorDiagnostic&& rhs ) noexcept
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_motor_id(rhs.get_motor_id());
      set_epoch(rhs.get_epoch());
      set_flags(rhs.get_flags());
      set_faults(rhs.get_faults());
      set_target_rad(rhs.get_target_rad());
      set_reference_rad(rhs.get_reference_rad());
      set_raw_count(rhs.get_raw_count());
      set_encoder_rad(rhs.get_encoder_rad());
      set_velocity_rad_s(rhs.get_velocity_rad_s());
      set_pwm_percent(rhs.get_pwm_percent());
      set_adc_raw(rhs.get_adc_raw());
      set_adc_voltage(rhs.get_adc_voltage());
      set_current_a(rhs.get_current_a());
      set_adc_state(rhs.get_adc_state());
      set_adc_error(rhs.get_adc_error());
      set_adc_age_ms(rhs.get_adc_age_ms());
      set_kalman_rad(rhs.get_kalman_rad());
      set_kalman_velocity_rad_s(rhs.get_kalman_velocity_rad_s());
      set_kalman_status(rhs.get_kalman_status());
    }

    ~MotorDiagnostic() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      TIMESTAMP_MS = 1,
      MOTOR_ID = 2,
      EPOCH = 3,
      FLAGS = 4,
      FAULTS = 5,
      TARGET_RAD = 6,
      REFERENCE_RAD = 7,
      RAW_COUNT = 8,
      ENCODER_RAD = 9,
      VELOCITY_RAD_S = 10,
      PWM_PERCENT = 11,
      ADC_RAW = 12,
      ADC_VOLTAGE = 13,
      CURRENT_A = 14,
      ADC_STATE = 15,
      ADC_ERROR = 16,
      ADC_AGE_MS = 17,
      KALMAN_RAD = 18,
      KALMAN_VELOCITY_RAD_S = 19,
      KALMAN_STATUS = 20
    };

    MotorDiagnostic& operator=(const MotorDiagnostic& rhs)
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_motor_id(rhs.get_motor_id());
      set_epoch(rhs.get_epoch());
      set_flags(rhs.get_flags());
      set_faults(rhs.get_faults());
      set_target_rad(rhs.get_target_rad());
      set_reference_rad(rhs.get_reference_rad());
      set_raw_count(rhs.get_raw_count());
      set_encoder_rad(rhs.get_encoder_rad());
      set_velocity_rad_s(rhs.get_velocity_rad_s());
      set_pwm_percent(rhs.get_pwm_percent());
      set_adc_raw(rhs.get_adc_raw());
      set_adc_voltage(rhs.get_adc_voltage());
      set_current_a(rhs.get_current_a());
      set_adc_state(rhs.get_adc_state());
      set_adc_error(rhs.get_adc_error());
      set_adc_age_ms(rhs.get_adc_age_ms());
      set_kalman_rad(rhs.get_kalman_rad());
      set_kalman_velocity_rad_s(rhs.get_kalman_velocity_rad_s());
      set_kalman_status(rhs.get_kalman_status());
      return *this;
    }

    MotorDiagnostic& operator=(const MotorDiagnostic&& rhs) noexcept
    {
      set_timestamp_ms(rhs.get_timestamp_ms());
      set_motor_id(rhs.get_motor_id());
      set_epoch(rhs.get_epoch());
      set_flags(rhs.get_flags());
      set_faults(rhs.get_faults());
      set_target_rad(rhs.get_target_rad());
      set_reference_rad(rhs.get_reference_rad());
      set_raw_count(rhs.get_raw_count());
      set_encoder_rad(rhs.get_encoder_rad());
      set_velocity_rad_s(rhs.get_velocity_rad_s());
      set_pwm_percent(rhs.get_pwm_percent());
      set_adc_raw(rhs.get_adc_raw());
      set_adc_voltage(rhs.get_adc_voltage());
      set_current_a(rhs.get_current_a());
      set_adc_state(rhs.get_adc_state());
      set_adc_error(rhs.get_adc_error());
      set_adc_age_ms(rhs.get_adc_age_ms());
      set_kalman_rad(rhs.get_kalman_rad());
      set_kalman_velocity_rad_s(rhs.get_kalman_velocity_rad_s());
      set_kalman_status(rhs.get_kalman_status());
      return *this;
    }

    static constexpr char const* TIMESTAMP_MS_NAME = "timestamp_ms";
    inline void clear_timestamp_ms() { timestamp_ms_.clear(); }
    inline void set_timestamp_ms(const uint32_t& value) { timestamp_ms_ = value; }
    inline void set_timestamp_ms(const uint32_t&& value) { timestamp_ms_ = value; }
    inline uint32_t& mutable_timestamp_ms() { return timestamp_ms_.get(); }
    inline const uint32_t& get_timestamp_ms() const { return timestamp_ms_.get(); }
    inline uint32_t timestamp_ms() const { return timestamp_ms_.get(); }

    static constexpr char const* MOTOR_ID_NAME = "motor_id";
    inline void clear_motor_id() { motor_id_.clear(); }
    inline void set_motor_id(const uint32_t& value) { motor_id_ = value; }
    inline void set_motor_id(const uint32_t&& value) { motor_id_ = value; }
    inline uint32_t& mutable_motor_id() { return motor_id_.get(); }
    inline const uint32_t& get_motor_id() const { return motor_id_.get(); }
    inline uint32_t motor_id() const { return motor_id_.get(); }

    static constexpr char const* EPOCH_NAME = "epoch";
    inline void clear_epoch() { epoch_.clear(); }
    inline void set_epoch(const uint32_t& value) { epoch_ = value; }
    inline void set_epoch(const uint32_t&& value) { epoch_ = value; }
    inline uint32_t& mutable_epoch() { return epoch_.get(); }
    inline const uint32_t& get_epoch() const { return epoch_.get(); }
    inline uint32_t epoch() const { return epoch_.get(); }

    static constexpr char const* FLAGS_NAME = "flags";
    inline void clear_flags() { flags_.clear(); }
    inline void set_flags(const uint32_t& value) { flags_ = value; }
    inline void set_flags(const uint32_t&& value) { flags_ = value; }
    inline uint32_t& mutable_flags() { return flags_.get(); }
    inline const uint32_t& get_flags() const { return flags_.get(); }
    inline uint32_t flags() const { return flags_.get(); }

    static constexpr char const* FAULTS_NAME = "faults";
    inline void clear_faults() { faults_.clear(); }
    inline void set_faults(const uint32_t& value) { faults_ = value; }
    inline void set_faults(const uint32_t&& value) { faults_ = value; }
    inline uint32_t& mutable_faults() { return faults_.get(); }
    inline const uint32_t& get_faults() const { return faults_.get(); }
    inline uint32_t faults() const { return faults_.get(); }

    static constexpr char const* TARGET_RAD_NAME = "target_rad";
    inline void clear_target_rad() { target_rad_.clear(); }
    inline void set_target_rad(const float& value) { target_rad_ = value; }
    inline void set_target_rad(const float&& value) { target_rad_ = value; }
    inline float& mutable_target_rad() { return target_rad_.get(); }
    inline const float& get_target_rad() const { return target_rad_.get(); }
    inline float target_rad() const { return target_rad_.get(); }

    static constexpr char const* REFERENCE_RAD_NAME = "reference_rad";
    inline void clear_reference_rad() { reference_rad_.clear(); }
    inline void set_reference_rad(const float& value) { reference_rad_ = value; }
    inline void set_reference_rad(const float&& value) { reference_rad_ = value; }
    inline float& mutable_reference_rad() { return reference_rad_.get(); }
    inline const float& get_reference_rad() const { return reference_rad_.get(); }
    inline float reference_rad() const { return reference_rad_.get(); }

    static constexpr char const* RAW_COUNT_NAME = "raw_count";
    inline void clear_raw_count() { raw_count_.clear(); }
    inline void set_raw_count(const int64_t& value) { raw_count_ = value; }
    inline void set_raw_count(const int64_t&& value) { raw_count_ = value; }
    inline int64_t& mutable_raw_count() { return raw_count_.get(); }
    inline const int64_t& get_raw_count() const { return raw_count_.get(); }
    inline int64_t raw_count() const { return raw_count_.get(); }

    static constexpr char const* ENCODER_RAD_NAME = "encoder_rad";
    inline void clear_encoder_rad() { encoder_rad_.clear(); }
    inline void set_encoder_rad(const float& value) { encoder_rad_ = value; }
    inline void set_encoder_rad(const float&& value) { encoder_rad_ = value; }
    inline float& mutable_encoder_rad() { return encoder_rad_.get(); }
    inline const float& get_encoder_rad() const { return encoder_rad_.get(); }
    inline float encoder_rad() const { return encoder_rad_.get(); }

    static constexpr char const* VELOCITY_RAD_S_NAME = "velocity_rad_s";
    inline void clear_velocity_rad_s() { velocity_rad_s_.clear(); }
    inline void set_velocity_rad_s(const float& value) { velocity_rad_s_ = value; }
    inline void set_velocity_rad_s(const float&& value) { velocity_rad_s_ = value; }
    inline float& mutable_velocity_rad_s() { return velocity_rad_s_.get(); }
    inline const float& get_velocity_rad_s() const { return velocity_rad_s_.get(); }
    inline float velocity_rad_s() const { return velocity_rad_s_.get(); }

    static constexpr char const* PWM_PERCENT_NAME = "pwm_percent";
    inline void clear_pwm_percent() { pwm_percent_.clear(); }
    inline void set_pwm_percent(const float& value) { pwm_percent_ = value; }
    inline void set_pwm_percent(const float&& value) { pwm_percent_ = value; }
    inline float& mutable_pwm_percent() { return pwm_percent_.get(); }
    inline const float& get_pwm_percent() const { return pwm_percent_.get(); }
    inline float pwm_percent() const { return pwm_percent_.get(); }

    static constexpr char const* ADC_RAW_NAME = "adc_raw";
    inline void clear_adc_raw() { adc_raw_.clear(); }
    inline void set_adc_raw(const uint32_t& value) { adc_raw_ = value; }
    inline void set_adc_raw(const uint32_t&& value) { adc_raw_ = value; }
    inline uint32_t& mutable_adc_raw() { return adc_raw_.get(); }
    inline const uint32_t& get_adc_raw() const { return adc_raw_.get(); }
    inline uint32_t adc_raw() const { return adc_raw_.get(); }

    static constexpr char const* ADC_VOLTAGE_NAME = "adc_voltage";
    inline void clear_adc_voltage() { adc_voltage_.clear(); }
    inline void set_adc_voltage(const float& value) { adc_voltage_ = value; }
    inline void set_adc_voltage(const float&& value) { adc_voltage_ = value; }
    inline float& mutable_adc_voltage() { return adc_voltage_.get(); }
    inline const float& get_adc_voltage() const { return adc_voltage_.get(); }
    inline float adc_voltage() const { return adc_voltage_.get(); }

    static constexpr char const* CURRENT_A_NAME = "current_a";
    inline void clear_current_a() { current_a_.clear(); }
    inline void set_current_a(const float& value) { current_a_ = value; }
    inline void set_current_a(const float&& value) { current_a_ = value; }
    inline float& mutable_current_a() { return current_a_.get(); }
    inline const float& get_current_a() const { return current_a_.get(); }
    inline float current_a() const { return current_a_.get(); }

    static constexpr char const* ADC_STATE_NAME = "adc_state";
    inline void clear_adc_state() { adc_state_.clear(); }
    inline void set_adc_state(const uint32_t& value) { adc_state_ = value; }
    inline void set_adc_state(const uint32_t&& value) { adc_state_ = value; }
    inline uint32_t& mutable_adc_state() { return adc_state_.get(); }
    inline const uint32_t& get_adc_state() const { return adc_state_.get(); }
    inline uint32_t adc_state() const { return adc_state_.get(); }

    static constexpr char const* ADC_ERROR_NAME = "adc_error";
    inline void clear_adc_error() { adc_error_.clear(); }
    inline void set_adc_error(const uint32_t& value) { adc_error_ = value; }
    inline void set_adc_error(const uint32_t&& value) { adc_error_ = value; }
    inline uint32_t& mutable_adc_error() { return adc_error_.get(); }
    inline const uint32_t& get_adc_error() const { return adc_error_.get(); }
    inline uint32_t adc_error() const { return adc_error_.get(); }

    static constexpr char const* ADC_AGE_MS_NAME = "adc_age_ms";
    inline void clear_adc_age_ms() { adc_age_ms_.clear(); }
    inline void set_adc_age_ms(const uint32_t& value) { adc_age_ms_ = value; }
    inline void set_adc_age_ms(const uint32_t&& value) { adc_age_ms_ = value; }
    inline uint32_t& mutable_adc_age_ms() { return adc_age_ms_.get(); }
    inline const uint32_t& get_adc_age_ms() const { return adc_age_ms_.get(); }
    inline uint32_t adc_age_ms() const { return adc_age_ms_.get(); }

    static constexpr char const* KALMAN_RAD_NAME = "kalman_rad";
    inline void clear_kalman_rad() { kalman_rad_.clear(); }
    inline void set_kalman_rad(const float& value) { kalman_rad_ = value; }
    inline void set_kalman_rad(const float&& value) { kalman_rad_ = value; }
    inline float& mutable_kalman_rad() { return kalman_rad_.get(); }
    inline const float& get_kalman_rad() const { return kalman_rad_.get(); }
    inline float kalman_rad() const { return kalman_rad_.get(); }

    static constexpr char const* KALMAN_VELOCITY_RAD_S_NAME = "kalman_velocity_rad_s";
    inline void clear_kalman_velocity_rad_s() { kalman_velocity_rad_s_.clear(); }
    inline void set_kalman_velocity_rad_s(const float& value) { kalman_velocity_rad_s_ = value; }
    inline void set_kalman_velocity_rad_s(const float&& value) { kalman_velocity_rad_s_ = value; }
    inline float& mutable_kalman_velocity_rad_s() { return kalman_velocity_rad_s_.get(); }
    inline const float& get_kalman_velocity_rad_s() const { return kalman_velocity_rad_s_.get(); }
    inline float kalman_velocity_rad_s() const { return kalman_velocity_rad_s_.get(); }

    static constexpr char const* KALMAN_STATUS_NAME = "kalman_status";
    inline void clear_kalman_status() { kalman_status_.clear(); }
    inline void set_kalman_status(const uint32_t& value) { kalman_status_ = value; }
    inline void set_kalman_status(const uint32_t&& value) { kalman_status_ = value; }
    inline uint32_t& mutable_kalman_status() { return kalman_status_.get(); }
    inline const uint32_t& get_kalman_status() const { return kalman_status_.get(); }
    inline uint32_t kalman_status() const { return kalman_status_.get(); }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != timestamp_ms_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = timestamp_ms_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TIMESTAMP_MS), buffer, false);
      }

      if((0U != motor_id_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = motor_id_.serialize_with_id(static_cast<uint32_t>(FieldNumber::MOTOR_ID), buffer, false);
      }

      if((0U != epoch_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = epoch_.serialize_with_id(static_cast<uint32_t>(FieldNumber::EPOCH), buffer, false);
      }

      if((0U != flags_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = flags_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FLAGS), buffer, false);
      }

      if((0U != faults_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = faults_.serialize_with_id(static_cast<uint32_t>(FieldNumber::FAULTS), buffer, false);
      }

      if((0.0 != target_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = target_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::TARGET_RAD), buffer, false);
      }

      if((0.0 != reference_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = reference_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REFERENCE_RAD), buffer, false);
      }

      if((0 != raw_count_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = raw_count_.serialize_with_id(static_cast<uint32_t>(FieldNumber::RAW_COUNT), buffer, false);
      }

      if((0.0 != encoder_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = encoder_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ENCODER_RAD), buffer, false);
      }

      if((0.0 != velocity_rad_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = velocity_rad_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::VELOCITY_RAD_S), buffer, false);
      }

      if((0.0 != pwm_percent_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = pwm_percent_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PWM_PERCENT), buffer, false);
      }

      if((0U != adc_raw_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = adc_raw_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ADC_RAW), buffer, false);
      }

      if((0.0 != adc_voltage_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = adc_voltage_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ADC_VOLTAGE), buffer, false);
      }

      if((0.0 != current_a_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = current_a_.serialize_with_id(static_cast<uint32_t>(FieldNumber::CURRENT_A), buffer, false);
      }

      if((0U != adc_state_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = adc_state_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ADC_STATE), buffer, false);
      }

      if((0U != adc_error_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = adc_error_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ADC_ERROR), buffer, false);
      }

      if((0U != adc_age_ms_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = adc_age_ms_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ADC_AGE_MS), buffer, false);
      }

      if((0.0 != kalman_rad_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = kalman_rad_.serialize_with_id(static_cast<uint32_t>(FieldNumber::KALMAN_RAD), buffer, false);
      }

      if((0.0 != kalman_velocity_rad_s_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = kalman_velocity_rad_s_.serialize_with_id(static_cast<uint32_t>(FieldNumber::KALMAN_VELOCITY_RAD_S), buffer, false);
      }

      if((0U != kalman_status_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = kalman_status_.serialize_with_id(static_cast<uint32_t>(FieldNumber::KALMAN_STATUS), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::TIMESTAMP_MS:
            return_value = timestamp_ms_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::MOTOR_ID:
            return_value = motor_id_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::EPOCH:
            return_value = epoch_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FLAGS:
            return_value = flags_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::FAULTS:
            return_value = faults_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::TARGET_RAD:
            return_value = target_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::REFERENCE_RAD:
            return_value = reference_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::RAW_COUNT:
            return_value = raw_count_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ENCODER_RAD:
            return_value = encoder_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::VELOCITY_RAD_S:
            return_value = velocity_rad_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PWM_PERCENT:
            return_value = pwm_percent_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ADC_RAW:
            return_value = adc_raw_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ADC_VOLTAGE:
            return_value = adc_voltage_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::CURRENT_A:
            return_value = current_a_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ADC_STATE:
            return_value = adc_state_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ADC_ERROR:
            return_value = adc_error_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::ADC_AGE_MS:
            return_value = adc_age_ms_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::KALMAN_RAD:
            return_value = kalman_rad_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::KALMAN_VELOCITY_RAD_S:
            return_value = kalman_velocity_rad_s_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::KALMAN_STATUS:
            return_value = kalman_status_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_timestamp_ms();
      clear_motor_id();
      clear_epoch();
      clear_flags();
      clear_faults();
      clear_target_rad();
      clear_reference_rad();
      clear_raw_count();
      clear_encoder_rad();
      clear_velocity_rad_s();
      clear_pwm_percent();
      clear_adc_raw();
      clear_adc_voltage();
      clear_current_a();
      clear_adc_state();
      clear_adc_error();
      clear_adc_age_ms();
      clear_kalman_rad();
      clear_kalman_velocity_rad_s();
      clear_kalman_status();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::TIMESTAMP_MS:
          name = TIMESTAMP_MS_NAME;
          break;
        case FieldNumber::MOTOR_ID:
          name = MOTOR_ID_NAME;
          break;
        case FieldNumber::EPOCH:
          name = EPOCH_NAME;
          break;
        case FieldNumber::FLAGS:
          name = FLAGS_NAME;
          break;
        case FieldNumber::FAULTS:
          name = FAULTS_NAME;
          break;
        case FieldNumber::TARGET_RAD:
          name = TARGET_RAD_NAME;
          break;
        case FieldNumber::REFERENCE_RAD:
          name = REFERENCE_RAD_NAME;
          break;
        case FieldNumber::RAW_COUNT:
          name = RAW_COUNT_NAME;
          break;
        case FieldNumber::ENCODER_RAD:
          name = ENCODER_RAD_NAME;
          break;
        case FieldNumber::VELOCITY_RAD_S:
          name = VELOCITY_RAD_S_NAME;
          break;
        case FieldNumber::PWM_PERCENT:
          name = PWM_PERCENT_NAME;
          break;
        case FieldNumber::ADC_RAW:
          name = ADC_RAW_NAME;
          break;
        case FieldNumber::ADC_VOLTAGE:
          name = ADC_VOLTAGE_NAME;
          break;
        case FieldNumber::CURRENT_A:
          name = CURRENT_A_NAME;
          break;
        case FieldNumber::ADC_STATE:
          name = ADC_STATE_NAME;
          break;
        case FieldNumber::ADC_ERROR:
          name = ADC_ERROR_NAME;
          break;
        case FieldNumber::ADC_AGE_MS:
          name = ADC_AGE_MS_NAME;
          break;
        case FieldNumber::KALMAN_RAD:
          name = KALMAN_RAD_NAME;
          break;
        case FieldNumber::KALMAN_VELOCITY_RAD_S:
          name = KALMAN_VELOCITY_RAD_S_NAME;
          break;
        case FieldNumber::KALMAN_STATUS:
          name = KALMAN_STATUS_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = timestamp_ms_.to_string(left_chars, indent_level + 2, TIMESTAMP_MS_NAME, true);
      left_chars = motor_id_.to_string(left_chars, indent_level + 2, MOTOR_ID_NAME, false);
      left_chars = epoch_.to_string(left_chars, indent_level + 2, EPOCH_NAME, false);
      left_chars = flags_.to_string(left_chars, indent_level + 2, FLAGS_NAME, false);
      left_chars = faults_.to_string(left_chars, indent_level + 2, FAULTS_NAME, false);
      left_chars = target_rad_.to_string(left_chars, indent_level + 2, TARGET_RAD_NAME, false);
      left_chars = reference_rad_.to_string(left_chars, indent_level + 2, REFERENCE_RAD_NAME, false);
      left_chars = raw_count_.to_string(left_chars, indent_level + 2, RAW_COUNT_NAME, false);
      left_chars = encoder_rad_.to_string(left_chars, indent_level + 2, ENCODER_RAD_NAME, false);
      left_chars = velocity_rad_s_.to_string(left_chars, indent_level + 2, VELOCITY_RAD_S_NAME, false);
      left_chars = pwm_percent_.to_string(left_chars, indent_level + 2, PWM_PERCENT_NAME, false);
      left_chars = adc_raw_.to_string(left_chars, indent_level + 2, ADC_RAW_NAME, false);
      left_chars = adc_voltage_.to_string(left_chars, indent_level + 2, ADC_VOLTAGE_NAME, false);
      left_chars = current_a_.to_string(left_chars, indent_level + 2, CURRENT_A_NAME, false);
      left_chars = adc_state_.to_string(left_chars, indent_level + 2, ADC_STATE_NAME, false);
      left_chars = adc_error_.to_string(left_chars, indent_level + 2, ADC_ERROR_NAME, false);
      left_chars = adc_age_ms_.to_string(left_chars, indent_level + 2, ADC_AGE_MS_NAME, false);
      left_chars = kalman_rad_.to_string(left_chars, indent_level + 2, KALMAN_RAD_NAME, false);
      left_chars = kalman_velocity_rad_s_.to_string(left_chars, indent_level + 2, KALMAN_VELOCITY_RAD_S_NAME, false);
      left_chars = kalman_status_.to_string(left_chars, indent_level + 2, KALMAN_STATUS_NAME, false);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 timestamp_ms_ = 0U;
      EmbeddedProto::uint32 motor_id_ = 0U;
      EmbeddedProto::uint32 epoch_ = 0U;
      EmbeddedProto::uint32 flags_ = 0U;
      EmbeddedProto::uint32 faults_ = 0U;
      EmbeddedProto::floatfixed target_rad_ = 0.0;
      EmbeddedProto::floatfixed reference_rad_ = 0.0;
      EmbeddedProto::sint64 raw_count_ = 0;
      EmbeddedProto::floatfixed encoder_rad_ = 0.0;
      EmbeddedProto::floatfixed velocity_rad_s_ = 0.0;
      EmbeddedProto::floatfixed pwm_percent_ = 0.0;
      EmbeddedProto::uint32 adc_raw_ = 0U;
      EmbeddedProto::floatfixed adc_voltage_ = 0.0;
      EmbeddedProto::floatfixed current_a_ = 0.0;
      EmbeddedProto::uint32 adc_state_ = 0U;
      EmbeddedProto::uint32 adc_error_ = 0U;
      EmbeddedProto::uint32 adc_age_ms_ = 0U;
      EmbeddedProto::floatfixed kalman_rad_ = 0.0;
      EmbeddedProto::floatfixed kalman_velocity_rad_s_ = 0.0;
      EmbeddedProto::uint32 kalman_status_ = 0U;

};

class StopRequest final: public ::EmbeddedProto::MessageInterface
{
  public:
    StopRequest() = default;
    StopRequest(const StopRequest& rhs )
    {
    }

    StopRequest(const StopRequest&& rhs ) noexcept
    {
    }

    ~StopRequest() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
    };

    StopRequest& operator=(const StopRequest& rhs)
    {
      return *this;
    }

    StopRequest& operator=(const StopRequest&& rhs) noexcept
    {
      return *this;
    }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }


      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:



};

class Data final: public ::EmbeddedProto::MessageInterface
{
  public:
    Data() = default;
    Data(const Data& rhs )
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::INSTRUCTION:
          set_instruction(rhs.get_instruction());
          break;

        default:
          break;
      }

    }

    Data(const Data&& rhs ) noexcept
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::INSTRUCTION:
          set_instruction(rhs.get_instruction());
          break;

        default:
          break;
      }

    }

    ~Data() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      INSTRUCTION = 1
    };

    Data& operator=(const Data& rhs)
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::INSTRUCTION:
          set_instruction(rhs.get_instruction());
          break;

        default:
          break;
      }

      return *this;
    }

    Data& operator=(const Data&& rhs) noexcept
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::INSTRUCTION:
          set_instruction(rhs.get_instruction());
          break;

        default:
          break;
      }

      return *this;
    }

    FieldNumber get_which_message() const { return which_message_; }

    static constexpr char const* INSTRUCTION_NAME = "instruction";
    inline bool has_instruction() const
    {
      return FieldNumber::INSTRUCTION == which_message_;
    }
    inline void clear_instruction()
    {
      if(FieldNumber::INSTRUCTION == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.instruction_.~ClassificationData();
      }
    }
    inline void set_instruction(const ClassificationData& value)
    {
      if(FieldNumber::INSTRUCTION != which_message_)
      {
        init_message(FieldNumber::INSTRUCTION);
      }
      message_.instruction_ = value;
    }
    inline void set_instruction(const ClassificationData&& value)
    {
      if(FieldNumber::INSTRUCTION != which_message_)
      {
        init_message(FieldNumber::INSTRUCTION);
      }
      message_.instruction_ = value;
    }
    inline ClassificationData& mutable_instruction()
    {
      if(FieldNumber::INSTRUCTION != which_message_)
      {
        init_message(FieldNumber::INSTRUCTION);
      }
      return message_.instruction_;
    }
    inline const ClassificationData& get_instruction() const { return message_.instruction_; }
    inline const ClassificationData& instruction() const { return message_.instruction_; }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      switch(which_message_)
      {
        case FieldNumber::INSTRUCTION:
          if(has_instruction() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.instruction_.serialize_with_id(static_cast<uint32_t>(FieldNumber::INSTRUCTION), buffer, true);
          }
          break;

        default:
          break;
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::INSTRUCTION:
            return_value = deserialize_message(id_tag, buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_message();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::INSTRUCTION:
          name = INSTRUCTION_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = to_string_message(left_chars, indent_level + 2, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:



      FieldNumber which_message_ = FieldNumber::NOT_SET;
      union message
      {
        message() {}
        ~message() {}
        ClassificationData instruction_;
      };
      message message_;

      void init_message(const FieldNumber field_id)
      {
        if(FieldNumber::NOT_SET != which_message_)
        {
          // First delete the old object in the oneof.
          clear_message();
        }

        // C++11 unions only support nontrivial members when you explicitly call the placement new statement.
        switch(field_id)
        {
          case FieldNumber::INSTRUCTION:
            new(&message_.instruction_) ClassificationData;
            break;

          default:
            break;
         }

         which_message_ = field_id;
      }

      void clear_message()
      {
        switch(which_message_)
        {
          case FieldNumber::INSTRUCTION:
            ::EmbeddedProto::destroy_at(&message_.instruction_);
            break;
          default:
            break;
        }
        which_message_ = FieldNumber::NOT_SET;
      }

      ::EmbeddedProto::Error deserialize_message(const FieldNumber field_id,
                                    ::EmbeddedProto::ReadBufferInterface& buffer,
                                    const ::EmbeddedProto::WireFormatter::WireType wire_type)
      {
        ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

        if(field_id != which_message_)
        {
          init_message(field_id);
        }

        switch(which_message_)
        {
          case FieldNumber::INSTRUCTION:
            return_value = message_.instruction_.deserialize_check_type(buffer, wire_type);
            break;
          default:
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS != return_value)
        {
          clear_message();
        }
        return return_value;
      }

#ifdef MSG_TO_STRING
      ::EmbeddedProto::string_view to_string_message(::EmbeddedProto::string_view& str, const uint32_t indent_level, const bool first_field) const
      {
        ::EmbeddedProto::string_view left_chars = str;

        switch(which_message_)
        {
          case FieldNumber::INSTRUCTION:
            left_chars = message_.instruction_.to_string(left_chars, indent_level, INSTRUCTION_NAME, first_field);
            break;
          default:
            break;
        }

        return left_chars;
      }

#endif // End of MSG_TO_STRING
};

template<
    uint32_t ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH
>
class ModeParams final: public ::EmbeddedProto::MessageInterface
{
  public:
    ModeParams() = default;
    ModeParams(const ModeParams& rhs )
    {
      if(rhs.get_which_mode_specific() != which_mode_specific_)
      {
        // First delete the old object in the oneof.
        clear_mode_specific();
      }

      switch(rhs.get_which_mode_specific())
      {
        case FieldNumber::CLASSIFICATION:
          set_classification(rhs.get_classification());
          break;

        case FieldNumber::REGRESSION:
          set_regression(rhs.get_regression());
          break;

        default:
          break;
      }

    }

    ModeParams(const ModeParams&& rhs ) noexcept
    {
      if(rhs.get_which_mode_specific() != which_mode_specific_)
      {
        // First delete the old object in the oneof.
        clear_mode_specific();
      }

      switch(rhs.get_which_mode_specific())
      {
        case FieldNumber::CLASSIFICATION:
          set_classification(rhs.get_classification());
          break;

        case FieldNumber::REGRESSION:
          set_regression(rhs.get_regression());
          break;

        default:
          break;
      }

    }

    ~ModeParams() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      CLASSIFICATION = 1,
      REGRESSION = 2
    };

    ModeParams& operator=(const ModeParams& rhs)
    {
      if(rhs.get_which_mode_specific() != which_mode_specific_)
      {
        // First delete the old object in the oneof.
        clear_mode_specific();
      }

      switch(rhs.get_which_mode_specific())
      {
        case FieldNumber::CLASSIFICATION:
          set_classification(rhs.get_classification());
          break;

        case FieldNumber::REGRESSION:
          set_regression(rhs.get_regression());
          break;

        default:
          break;
      }

      return *this;
    }

    ModeParams& operator=(const ModeParams&& rhs) noexcept
    {
      if(rhs.get_which_mode_specific() != which_mode_specific_)
      {
        // First delete the old object in the oneof.
        clear_mode_specific();
      }

      switch(rhs.get_which_mode_specific())
      {
        case FieldNumber::CLASSIFICATION:
          set_classification(rhs.get_classification());
          break;

        case FieldNumber::REGRESSION:
          set_regression(rhs.get_regression());
          break;

        default:
          break;
      }

      return *this;
    }

    FieldNumber get_which_mode_specific() const { return which_mode_specific_; }

    static constexpr char const* CLASSIFICATION_NAME = "classification";
    inline bool has_classification() const
    {
      return FieldNumber::CLASSIFICATION == which_mode_specific_;
    }
    inline void clear_classification()
    {
      if(FieldNumber::CLASSIFICATION == which_mode_specific_)
      {
        which_mode_specific_ = FieldNumber::NOT_SET;
        mode_specific_.classification_.~ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>();
      }
    }
    inline void set_classification(const ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& value)
    {
      if(FieldNumber::CLASSIFICATION != which_mode_specific_)
      {
        init_mode_specific(FieldNumber::CLASSIFICATION);
      }
      mode_specific_.classification_ = value;
    }
    inline void set_classification(const ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>&& value)
    {
      if(FieldNumber::CLASSIFICATION != which_mode_specific_)
      {
        init_mode_specific(FieldNumber::CLASSIFICATION);
      }
      mode_specific_.classification_ = value;
    }
    inline ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& mutable_classification()
    {
      if(FieldNumber::CLASSIFICATION != which_mode_specific_)
      {
        init_mode_specific(FieldNumber::CLASSIFICATION);
      }
      return mode_specific_.classification_;
    }
    inline const ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& get_classification() const { return mode_specific_.classification_; }
    inline const ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& classification() const { return mode_specific_.classification_; }

    static constexpr char const* REGRESSION_NAME = "regression";
    inline bool has_regression() const
    {
      return FieldNumber::REGRESSION == which_mode_specific_;
    }
    inline void clear_regression()
    {
      if(FieldNumber::REGRESSION == which_mode_specific_)
      {
        which_mode_specific_ = FieldNumber::NOT_SET;
        mode_specific_.regression_.~LogisticRegressionParams();
      }
    }
    inline void set_regression(const LogisticRegressionParams& value)
    {
      if(FieldNumber::REGRESSION != which_mode_specific_)
      {
        init_mode_specific(FieldNumber::REGRESSION);
      }
      mode_specific_.regression_ = value;
    }
    inline void set_regression(const LogisticRegressionParams&& value)
    {
      if(FieldNumber::REGRESSION != which_mode_specific_)
      {
        init_mode_specific(FieldNumber::REGRESSION);
      }
      mode_specific_.regression_ = value;
    }
    inline LogisticRegressionParams& mutable_regression()
    {
      if(FieldNumber::REGRESSION != which_mode_specific_)
      {
        init_mode_specific(FieldNumber::REGRESSION);
      }
      return mode_specific_.regression_;
    }
    inline const LogisticRegressionParams& get_regression() const { return mode_specific_.regression_; }
    inline const LogisticRegressionParams& regression() const { return mode_specific_.regression_; }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      switch(which_mode_specific_)
      {
        case FieldNumber::CLASSIFICATION:
          if(has_classification() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = mode_specific_.classification_.serialize_with_id(static_cast<uint32_t>(FieldNumber::CLASSIFICATION), buffer, true);
          }
          break;

        case FieldNumber::REGRESSION:
          if(has_regression() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = mode_specific_.regression_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REGRESSION), buffer, true);
          }
          break;

        default:
          break;
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::CLASSIFICATION:
          case FieldNumber::REGRESSION:
            return_value = deserialize_mode_specific(id_tag, buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_mode_specific();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::CLASSIFICATION:
          name = CLASSIFICATION_NAME;
          break;
        case FieldNumber::REGRESSION:
          name = REGRESSION_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = to_string_mode_specific(left_chars, indent_level + 2, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:



      FieldNumber which_mode_specific_ = FieldNumber::NOT_SET;
      union mode_specific
      {
        mode_specific() {}
        ~mode_specific() {}
        ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH> classification_;
        LogisticRegressionParams regression_;
      };
      mode_specific mode_specific_;

      void init_mode_specific(const FieldNumber field_id)
      {
        if(FieldNumber::NOT_SET != which_mode_specific_)
        {
          // First delete the old object in the oneof.
          clear_mode_specific();
        }

        // C++11 unions only support nontrivial members when you explicitly call the placement new statement.
        switch(field_id)
        {
          case FieldNumber::CLASSIFICATION:
            new(&mode_specific_.classification_) ClassificationParams<ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>;
            break;

          case FieldNumber::REGRESSION:
            new(&mode_specific_.regression_) LogisticRegressionParams;
            break;

          default:
            break;
         }

         which_mode_specific_ = field_id;
      }

      void clear_mode_specific()
      {
        switch(which_mode_specific_)
        {
          case FieldNumber::CLASSIFICATION:
            ::EmbeddedProto::destroy_at(&mode_specific_.classification_);
            break;
          case FieldNumber::REGRESSION:
            ::EmbeddedProto::destroy_at(&mode_specific_.regression_);
            break;
          default:
            break;
        }
        which_mode_specific_ = FieldNumber::NOT_SET;
      }

      ::EmbeddedProto::Error deserialize_mode_specific(const FieldNumber field_id,
                                    ::EmbeddedProto::ReadBufferInterface& buffer,
                                    const ::EmbeddedProto::WireFormatter::WireType wire_type)
      {
        ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

        if(field_id != which_mode_specific_)
        {
          init_mode_specific(field_id);
        }

        switch(which_mode_specific_)
        {
          case FieldNumber::CLASSIFICATION:
            return_value = mode_specific_.classification_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::REGRESSION:
            return_value = mode_specific_.regression_.deserialize_check_type(buffer, wire_type);
            break;
          default:
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS != return_value)
        {
          clear_mode_specific();
        }
        return return_value;
      }

#ifdef MSG_TO_STRING
      ::EmbeddedProto::string_view to_string_mode_specific(::EmbeddedProto::string_view& str, const uint32_t indent_level, const bool first_field) const
      {
        ::EmbeddedProto::string_view left_chars = str;

        switch(which_mode_specific_)
        {
          case FieldNumber::CLASSIFICATION:
            left_chars = mode_specific_.classification_.to_string(left_chars, indent_level, CLASSIFICATION_NAME, first_field);
            break;
          case FieldNumber::REGRESSION:
            left_chars = mode_specific_.regression_.to_string(left_chars, indent_level, REGRESSION_NAME, first_field);
            break;
          default:
            break;
        }

        return left_chars;
      }

#endif // End of MSG_TO_STRING
};

template<
    uint32_t Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH
>
class Config final: public ::EmbeddedProto::MessageInterface
{
  public:
    Config() = default;
    Config(const Config& rhs )
    {
      set_selected_mode(rhs.get_selected_mode());
      set_params(rhs.get_params());
    }

    Config(const Config&& rhs ) noexcept
    {
      set_selected_mode(rhs.get_selected_mode());
      set_params(rhs.get_params());
    }

    ~Config() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      SELECTED_MODE = 1,
      PARAMS = 2
    };

    Config& operator=(const Config& rhs)
    {
      set_selected_mode(rhs.get_selected_mode());
      set_params(rhs.get_params());
      return *this;
    }

    Config& operator=(const Config&& rhs) noexcept
    {
      set_selected_mode(rhs.get_selected_mode());
      set_params(rhs.get_params());
      return *this;
    }

    static constexpr char const* SELECTED_MODE_NAME = "selected_mode";
    inline void clear_selected_mode() { selected_mode_.clear(); }
    inline void set_selected_mode(const uint32_t& value) { selected_mode_ = value; }
    inline void set_selected_mode(const uint32_t&& value) { selected_mode_ = value; }
    inline uint32_t& mutable_selected_mode() { return selected_mode_.get(); }
    inline const uint32_t& get_selected_mode() const { return selected_mode_.get(); }
    inline uint32_t selected_mode() const { return selected_mode_.get(); }

    static constexpr char const* PARAMS_NAME = "params";
    inline void clear_params() { params_.clear(); }
    inline void set_params(const ModeParams<Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& value) { params_ = value; }
    inline void set_params(const ModeParams<Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>&& value) { params_ = value; }
    inline ModeParams<Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& mutable_params() { return params_; }
    inline const ModeParams<Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& get_params() const { return params_; }
    inline const ModeParams<Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& params() const { return params_; }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      if((0U != selected_mode_.get()) && (::EmbeddedProto::Error::NO_ERRORS == return_value))
      {
        return_value = selected_mode_.serialize_with_id(static_cast<uint32_t>(FieldNumber::SELECTED_MODE), buffer, false);
      }

      if(::EmbeddedProto::Error::NO_ERRORS == return_value)
      {
        return_value = params_.serialize_with_id(static_cast<uint32_t>(FieldNumber::PARAMS), buffer, false);
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::SELECTED_MODE:
            return_value = selected_mode_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::PARAMS:
            return_value = params_.deserialize_check_type(buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_selected_mode();
      clear_params();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::SELECTED_MODE:
          name = SELECTED_MODE_NAME;
          break;
        case FieldNumber::PARAMS:
          name = PARAMS_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = selected_mode_.to_string(left_chars, indent_level + 2, SELECTED_MODE_NAME, true);
      left_chars = params_.to_string(left_chars, indent_level + 2, PARAMS_NAME, false);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:


      EmbeddedProto::uint32 selected_mode_ = 0U;
      ModeParams<Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH> params_;

};

class DeviceMessage final: public ::EmbeddedProto::MessageInterface
{
  public:
    DeviceMessage() = default;
    DeviceMessage(const DeviceMessage& rhs )
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::HELLO:
          set_hello(rhs.get_hello());
          break;

        case FieldNumber::DATA:
          set_data(rhs.get_data());
          break;

        case FieldNumber::COMMISSION:
          set_commission(rhs.get_commission());
          break;

        case FieldNumber::STOP:
          set_stop(rhs.get_stop());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        default:
          break;
      }

    }

    DeviceMessage(const DeviceMessage&& rhs ) noexcept
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::HELLO:
          set_hello(rhs.get_hello());
          break;

        case FieldNumber::DATA:
          set_data(rhs.get_data());
          break;

        case FieldNumber::COMMISSION:
          set_commission(rhs.get_commission());
          break;

        case FieldNumber::STOP:
          set_stop(rhs.get_stop());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        default:
          break;
      }

    }

    ~DeviceMessage() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      HELLO = 1,
      DATA = 2,
      COMMISSION = 3,
      STOP = 4,
      DIAGNOSTIC = 5
    };

    DeviceMessage& operator=(const DeviceMessage& rhs)
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::HELLO:
          set_hello(rhs.get_hello());
          break;

        case FieldNumber::DATA:
          set_data(rhs.get_data());
          break;

        case FieldNumber::COMMISSION:
          set_commission(rhs.get_commission());
          break;

        case FieldNumber::STOP:
          set_stop(rhs.get_stop());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        default:
          break;
      }

      return *this;
    }

    DeviceMessage& operator=(const DeviceMessage&& rhs) noexcept
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::HELLO:
          set_hello(rhs.get_hello());
          break;

        case FieldNumber::DATA:
          set_data(rhs.get_data());
          break;

        case FieldNumber::COMMISSION:
          set_commission(rhs.get_commission());
          break;

        case FieldNumber::STOP:
          set_stop(rhs.get_stop());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        default:
          break;
      }

      return *this;
    }

    FieldNumber get_which_message() const { return which_message_; }

    static constexpr char const* HELLO_NAME = "hello";
    inline bool has_hello() const
    {
      return FieldNumber::HELLO == which_message_;
    }
    inline void clear_hello()
    {
      if(FieldNumber::HELLO == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.hello_.~Hello();
      }
    }
    inline void set_hello(const Hello& value)
    {
      if(FieldNumber::HELLO != which_message_)
      {
        init_message(FieldNumber::HELLO);
      }
      message_.hello_ = value;
    }
    inline void set_hello(const Hello&& value)
    {
      if(FieldNumber::HELLO != which_message_)
      {
        init_message(FieldNumber::HELLO);
      }
      message_.hello_ = value;
    }
    inline Hello& mutable_hello()
    {
      if(FieldNumber::HELLO != which_message_)
      {
        init_message(FieldNumber::HELLO);
      }
      return message_.hello_;
    }
    inline const Hello& get_hello() const { return message_.hello_; }
    inline const Hello& hello() const { return message_.hello_; }

    static constexpr char const* DATA_NAME = "data";
    inline bool has_data() const
    {
      return FieldNumber::DATA == which_message_;
    }
    inline void clear_data()
    {
      if(FieldNumber::DATA == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.data_.~Data();
      }
    }
    inline void set_data(const Data& value)
    {
      if(FieldNumber::DATA != which_message_)
      {
        init_message(FieldNumber::DATA);
      }
      message_.data_ = value;
    }
    inline void set_data(const Data&& value)
    {
      if(FieldNumber::DATA != which_message_)
      {
        init_message(FieldNumber::DATA);
      }
      message_.data_ = value;
    }
    inline Data& mutable_data()
    {
      if(FieldNumber::DATA != which_message_)
      {
        init_message(FieldNumber::DATA);
      }
      return message_.data_;
    }
    inline const Data& get_data() const { return message_.data_; }
    inline const Data& data() const { return message_.data_; }

    static constexpr char const* COMMISSION_NAME = "commission";
    inline bool has_commission() const
    {
      return FieldNumber::COMMISSION == which_message_;
    }
    inline void clear_commission()
    {
      if(FieldNumber::COMMISSION == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.commission_.~CommissionCommand();
      }
    }
    inline void set_commission(const CommissionCommand& value)
    {
      if(FieldNumber::COMMISSION != which_message_)
      {
        init_message(FieldNumber::COMMISSION);
      }
      message_.commission_ = value;
    }
    inline void set_commission(const CommissionCommand&& value)
    {
      if(FieldNumber::COMMISSION != which_message_)
      {
        init_message(FieldNumber::COMMISSION);
      }
      message_.commission_ = value;
    }
    inline CommissionCommand& mutable_commission()
    {
      if(FieldNumber::COMMISSION != which_message_)
      {
        init_message(FieldNumber::COMMISSION);
      }
      return message_.commission_;
    }
    inline const CommissionCommand& get_commission() const { return message_.commission_; }
    inline const CommissionCommand& commission() const { return message_.commission_; }

    static constexpr char const* STOP_NAME = "stop";
    inline bool has_stop() const
    {
      return FieldNumber::STOP == which_message_;
    }
    inline void clear_stop()
    {
      if(FieldNumber::STOP == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.stop_.~StopRequest();
      }
    }
    inline void set_stop(const StopRequest& value)
    {
      if(FieldNumber::STOP != which_message_)
      {
        init_message(FieldNumber::STOP);
      }
      message_.stop_ = value;
    }
    inline void set_stop(const StopRequest&& value)
    {
      if(FieldNumber::STOP != which_message_)
      {
        init_message(FieldNumber::STOP);
      }
      message_.stop_ = value;
    }
    inline StopRequest& mutable_stop()
    {
      if(FieldNumber::STOP != which_message_)
      {
        init_message(FieldNumber::STOP);
      }
      return message_.stop_;
    }
    inline const StopRequest& get_stop() const { return message_.stop_; }
    inline const StopRequest& stop() const { return message_.stop_; }

    static constexpr char const* DIAGNOSTIC_NAME = "diagnostic";
    inline bool has_diagnostic() const
    {
      return FieldNumber::DIAGNOSTIC == which_message_;
    }
    inline void clear_diagnostic()
    {
      if(FieldNumber::DIAGNOSTIC == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.diagnostic_.~DiagnosticRequest();
      }
    }
    inline void set_diagnostic(const DiagnosticRequest& value)
    {
      if(FieldNumber::DIAGNOSTIC != which_message_)
      {
        init_message(FieldNumber::DIAGNOSTIC);
      }
      message_.diagnostic_ = value;
    }
    inline void set_diagnostic(const DiagnosticRequest&& value)
    {
      if(FieldNumber::DIAGNOSTIC != which_message_)
      {
        init_message(FieldNumber::DIAGNOSTIC);
      }
      message_.diagnostic_ = value;
    }
    inline DiagnosticRequest& mutable_diagnostic()
    {
      if(FieldNumber::DIAGNOSTIC != which_message_)
      {
        init_message(FieldNumber::DIAGNOSTIC);
      }
      return message_.diagnostic_;
    }
    inline const DiagnosticRequest& get_diagnostic() const { return message_.diagnostic_; }
    inline const DiagnosticRequest& diagnostic() const { return message_.diagnostic_; }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      switch(which_message_)
      {
        case FieldNumber::HELLO:
          if(has_hello() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.hello_.serialize_with_id(static_cast<uint32_t>(FieldNumber::HELLO), buffer, true);
          }
          break;

        case FieldNumber::DATA:
          if(has_data() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.data_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DATA), buffer, true);
          }
          break;

        case FieldNumber::COMMISSION:
          if(has_commission() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.commission_.serialize_with_id(static_cast<uint32_t>(FieldNumber::COMMISSION), buffer, true);
          }
          break;

        case FieldNumber::STOP:
          if(has_stop() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.stop_.serialize_with_id(static_cast<uint32_t>(FieldNumber::STOP), buffer, true);
          }
          break;

        case FieldNumber::DIAGNOSTIC:
          if(has_diagnostic() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.diagnostic_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DIAGNOSTIC), buffer, true);
          }
          break;

        default:
          break;
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::HELLO:
          case FieldNumber::DATA:
          case FieldNumber::COMMISSION:
          case FieldNumber::STOP:
          case FieldNumber::DIAGNOSTIC:
            return_value = deserialize_message(id_tag, buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_message();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::HELLO:
          name = HELLO_NAME;
          break;
        case FieldNumber::DATA:
          name = DATA_NAME;
          break;
        case FieldNumber::COMMISSION:
          name = COMMISSION_NAME;
          break;
        case FieldNumber::STOP:
          name = STOP_NAME;
          break;
        case FieldNumber::DIAGNOSTIC:
          name = DIAGNOSTIC_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = to_string_message(left_chars, indent_level + 2, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:



      FieldNumber which_message_ = FieldNumber::NOT_SET;
      union message
      {
        message() {}
        ~message() {}
        Hello hello_;
        Data data_;
        CommissionCommand commission_;
        StopRequest stop_;
        DiagnosticRequest diagnostic_;
      };
      message message_;

      void init_message(const FieldNumber field_id)
      {
        if(FieldNumber::NOT_SET != which_message_)
        {
          // First delete the old object in the oneof.
          clear_message();
        }

        // C++11 unions only support nontrivial members when you explicitly call the placement new statement.
        switch(field_id)
        {
          case FieldNumber::HELLO:
            new(&message_.hello_) Hello;
            break;

          case FieldNumber::DATA:
            new(&message_.data_) Data;
            break;

          case FieldNumber::COMMISSION:
            new(&message_.commission_) CommissionCommand;
            break;

          case FieldNumber::STOP:
            new(&message_.stop_) StopRequest;
            break;

          case FieldNumber::DIAGNOSTIC:
            new(&message_.diagnostic_) DiagnosticRequest;
            break;

          default:
            break;
         }

         which_message_ = field_id;
      }

      void clear_message()
      {
        switch(which_message_)
        {
          case FieldNumber::HELLO:
            ::EmbeddedProto::destroy_at(&message_.hello_);
            break;
          case FieldNumber::DATA:
            ::EmbeddedProto::destroy_at(&message_.data_);
            break;
          case FieldNumber::COMMISSION:
            ::EmbeddedProto::destroy_at(&message_.commission_);
            break;
          case FieldNumber::STOP:
            ::EmbeddedProto::destroy_at(&message_.stop_);
            break;
          case FieldNumber::DIAGNOSTIC:
            ::EmbeddedProto::destroy_at(&message_.diagnostic_);
            break;
          default:
            break;
        }
        which_message_ = FieldNumber::NOT_SET;
      }

      ::EmbeddedProto::Error deserialize_message(const FieldNumber field_id,
                                    ::EmbeddedProto::ReadBufferInterface& buffer,
                                    const ::EmbeddedProto::WireFormatter::WireType wire_type)
      {
        ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

        if(field_id != which_message_)
        {
          init_message(field_id);
        }

        switch(which_message_)
        {
          case FieldNumber::HELLO:
            return_value = message_.hello_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::DATA:
            return_value = message_.data_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::COMMISSION:
            return_value = message_.commission_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::STOP:
            return_value = message_.stop_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::DIAGNOSTIC:
            return_value = message_.diagnostic_.deserialize_check_type(buffer, wire_type);
            break;
          default:
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS != return_value)
        {
          clear_message();
        }
        return return_value;
      }

#ifdef MSG_TO_STRING
      ::EmbeddedProto::string_view to_string_message(::EmbeddedProto::string_view& str, const uint32_t indent_level, const bool first_field) const
      {
        ::EmbeddedProto::string_view left_chars = str;

        switch(which_message_)
        {
          case FieldNumber::HELLO:
            left_chars = message_.hello_.to_string(left_chars, indent_level, HELLO_NAME, first_field);
            break;
          case FieldNumber::DATA:
            left_chars = message_.data_.to_string(left_chars, indent_level, DATA_NAME, first_field);
            break;
          case FieldNumber::COMMISSION:
            left_chars = message_.commission_.to_string(left_chars, indent_level, COMMISSION_NAME, first_field);
            break;
          case FieldNumber::STOP:
            left_chars = message_.stop_.to_string(left_chars, indent_level, STOP_NAME, first_field);
            break;
          case FieldNumber::DIAGNOSTIC:
            left_chars = message_.diagnostic_.to_string(left_chars, indent_level, DIAGNOSTIC_NAME, first_field);
            break;
          default:
            break;
        }

        return left_chars;
      }

#endif // End of MSG_TO_STRING
};

template<
    uint32_t ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH
>
class ProsthesisMessage final: public ::EmbeddedProto::MessageInterface
{
  public:
    ProsthesisMessage() = default;
    ProsthesisMessage(const ProsthesisMessage& rhs )
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::CONFIG:
          set_config(rhs.get_config());
          break;

        case FieldNumber::ERROR:
          set_error(rhs.get_error());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        case FieldNumber::REPLY:
          set_reply(rhs.get_reply());
          break;

        default:
          break;
      }

    }

    ProsthesisMessage(const ProsthesisMessage&& rhs ) noexcept
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::CONFIG:
          set_config(rhs.get_config());
          break;

        case FieldNumber::ERROR:
          set_error(rhs.get_error());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        case FieldNumber::REPLY:
          set_reply(rhs.get_reply());
          break;

        default:
          break;
      }

    }

    ~ProsthesisMessage() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      CONFIG = 1,
      ERROR = 2,
      DIAGNOSTIC = 3,
      REPLY = 4
    };

    ProsthesisMessage& operator=(const ProsthesisMessage& rhs)
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::CONFIG:
          set_config(rhs.get_config());
          break;

        case FieldNumber::ERROR:
          set_error(rhs.get_error());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        case FieldNumber::REPLY:
          set_reply(rhs.get_reply());
          break;

        default:
          break;
      }

      return *this;
    }

    ProsthesisMessage& operator=(const ProsthesisMessage&& rhs) noexcept
    {
      if(rhs.get_which_message() != which_message_)
      {
        // First delete the old object in the oneof.
        clear_message();
      }

      switch(rhs.get_which_message())
      {
        case FieldNumber::CONFIG:
          set_config(rhs.get_config());
          break;

        case FieldNumber::ERROR:
          set_error(rhs.get_error());
          break;

        case FieldNumber::DIAGNOSTIC:
          set_diagnostic(rhs.get_diagnostic());
          break;

        case FieldNumber::REPLY:
          set_reply(rhs.get_reply());
          break;

        default:
          break;
      }

      return *this;
    }

    FieldNumber get_which_message() const { return which_message_; }

    static constexpr char const* CONFIG_NAME = "config";
    inline bool has_config() const
    {
      return FieldNumber::CONFIG == which_message_;
    }
    inline void clear_config()
    {
      if(FieldNumber::CONFIG == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.config_.~Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>();
      }
    }
    inline void set_config(const Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& value)
    {
      if(FieldNumber::CONFIG != which_message_)
      {
        init_message(FieldNumber::CONFIG);
      }
      message_.config_ = value;
    }
    inline void set_config(const Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>&& value)
    {
      if(FieldNumber::CONFIG != which_message_)
      {
        init_message(FieldNumber::CONFIG);
      }
      message_.config_ = value;
    }
    inline Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& mutable_config()
    {
      if(FieldNumber::CONFIG != which_message_)
      {
        init_message(FieldNumber::CONFIG);
      }
      return message_.config_;
    }
    inline const Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& get_config() const { return message_.config_; }
    inline const Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>& config() const { return message_.config_; }

    static constexpr char const* ERROR_NAME = "error";
    inline bool has_error() const
    {
      return FieldNumber::ERROR == which_message_;
    }
    inline void clear_error()
    {
      if(FieldNumber::ERROR == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.error_.~Error();
      }
    }
    inline void set_error(const Error& value)
    {
      if(FieldNumber::ERROR != which_message_)
      {
        init_message(FieldNumber::ERROR);
      }
      message_.error_ = value;
    }
    inline void set_error(const Error&& value)
    {
      if(FieldNumber::ERROR != which_message_)
      {
        init_message(FieldNumber::ERROR);
      }
      message_.error_ = value;
    }
    inline Error& mutable_error()
    {
      if(FieldNumber::ERROR != which_message_)
      {
        init_message(FieldNumber::ERROR);
      }
      return message_.error_;
    }
    inline const Error& get_error() const { return message_.error_; }
    inline const Error& error() const { return message_.error_; }

    static constexpr char const* DIAGNOSTIC_NAME = "diagnostic";
    inline bool has_diagnostic() const
    {
      return FieldNumber::DIAGNOSTIC == which_message_;
    }
    inline void clear_diagnostic()
    {
      if(FieldNumber::DIAGNOSTIC == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.diagnostic_.~MotorDiagnostic();
      }
    }
    inline void set_diagnostic(const MotorDiagnostic& value)
    {
      if(FieldNumber::DIAGNOSTIC != which_message_)
      {
        init_message(FieldNumber::DIAGNOSTIC);
      }
      message_.diagnostic_ = value;
    }
    inline void set_diagnostic(const MotorDiagnostic&& value)
    {
      if(FieldNumber::DIAGNOSTIC != which_message_)
      {
        init_message(FieldNumber::DIAGNOSTIC);
      }
      message_.diagnostic_ = value;
    }
    inline MotorDiagnostic& mutable_diagnostic()
    {
      if(FieldNumber::DIAGNOSTIC != which_message_)
      {
        init_message(FieldNumber::DIAGNOSTIC);
      }
      return message_.diagnostic_;
    }
    inline const MotorDiagnostic& get_diagnostic() const { return message_.diagnostic_; }
    inline const MotorDiagnostic& diagnostic() const { return message_.diagnostic_; }

    static constexpr char const* REPLY_NAME = "reply";
    inline bool has_reply() const
    {
      return FieldNumber::REPLY == which_message_;
    }
    inline void clear_reply()
    {
      if(FieldNumber::REPLY == which_message_)
      {
        which_message_ = FieldNumber::NOT_SET;
        message_.reply_.~CommandReply();
      }
    }
    inline void set_reply(const CommandReply& value)
    {
      if(FieldNumber::REPLY != which_message_)
      {
        init_message(FieldNumber::REPLY);
      }
      message_.reply_ = value;
    }
    inline void set_reply(const CommandReply&& value)
    {
      if(FieldNumber::REPLY != which_message_)
      {
        init_message(FieldNumber::REPLY);
      }
      message_.reply_ = value;
    }
    inline CommandReply& mutable_reply()
    {
      if(FieldNumber::REPLY != which_message_)
      {
        init_message(FieldNumber::REPLY);
      }
      return message_.reply_;
    }
    inline const CommandReply& get_reply() const { return message_.reply_; }
    inline const CommandReply& reply() const { return message_.reply_; }


    ::EmbeddedProto::Error serialize(::EmbeddedProto::WriteBufferInterface& buffer) const override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

      switch(which_message_)
      {
        case FieldNumber::CONFIG:
          if(has_config() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.config_.serialize_with_id(static_cast<uint32_t>(FieldNumber::CONFIG), buffer, true);
          }
          break;

        case FieldNumber::ERROR:
          if(has_error() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.error_.serialize_with_id(static_cast<uint32_t>(FieldNumber::ERROR), buffer, true);
          }
          break;

        case FieldNumber::DIAGNOSTIC:
          if(has_diagnostic() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.diagnostic_.serialize_with_id(static_cast<uint32_t>(FieldNumber::DIAGNOSTIC), buffer, true);
          }
          break;

        case FieldNumber::REPLY:
          if(has_reply() && (::EmbeddedProto::Error::NO_ERRORS == return_value))
          {
            return_value = message_.reply_.serialize_with_id(static_cast<uint32_t>(FieldNumber::REPLY), buffer, true);
          }
          break;

        default:
          break;
      }

      return return_value;
    };

    ::EmbeddedProto::Error deserialize(::EmbeddedProto::ReadBufferInterface& buffer) override
    {
      ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;
      ::EmbeddedProto::WireFormatter::WireType wire_type = ::EmbeddedProto::WireFormatter::WireType::VARINT;
      uint32_t id_number = 0;
      FieldNumber id_tag = FieldNumber::NOT_SET;

      ::EmbeddedProto::Error tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
      while((::EmbeddedProto::Error::NO_ERRORS == return_value) && (::EmbeddedProto::Error::NO_ERRORS == tag_value))
      {
        id_tag = static_cast<FieldNumber>(id_number);
        switch(id_tag)
        {
          case FieldNumber::CONFIG:
          case FieldNumber::ERROR:
          case FieldNumber::DIAGNOSTIC:
          case FieldNumber::REPLY:
            return_value = deserialize_message(id_tag, buffer, wire_type);
            break;

          case FieldNumber::NOT_SET:
            return_value = ::EmbeddedProto::Error::INVALID_FIELD_ID;
            break;

          default:
            return_value = skip_unknown_field(buffer, wire_type);
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS == return_value)
        {
          // Read the next tag.
          tag_value = ::EmbeddedProto::WireFormatter::DeserializeTag(buffer, wire_type, id_number);
        }
      }

      // When an error was detect while reading the tag but no other errors where found, set it in the return value.
      if((::EmbeddedProto::Error::NO_ERRORS == return_value)
         && (::EmbeddedProto::Error::NO_ERRORS != tag_value)
         && (::EmbeddedProto::Error::END_OF_BUFFER != tag_value)) // The end of the buffer is not an array in this case.
      {
        return_value = tag_value;
      }

      return return_value;
    };

    void clear() override
    {
      clear_message();

    }

#ifndef DISABLE_FIELD_NUMBER_TO_NAME

    static char const* field_number_to_name(const FieldNumber fieldNumber)
    {
      char const* name = nullptr;
      switch(fieldNumber)
      {
        case FieldNumber::CONFIG:
          name = CONFIG_NAME;
          break;
        case FieldNumber::ERROR:
          name = ERROR_NAME;
          break;
        case FieldNumber::DIAGNOSTIC:
          name = DIAGNOSTIC_NAME;
          break;
        case FieldNumber::REPLY:
          name = REPLY_NAME;
          break;
        default:
          name = "Invalid FieldNumber";
          break;
      }
      return name;
    }

#endif

#ifdef MSG_TO_STRING

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str) const
    {
      return this->to_string(str, 0, nullptr, true);
    }

    ::EmbeddedProto::string_view to_string(::EmbeddedProto::string_view& str, const uint32_t indent_level, char const* name, const bool first_field) const override
    {
      ::EmbeddedProto::string_view left_chars = str;
      int32_t n_chars_used = 0;

      if(!first_field)
      {
        // Add a comma behind the previous field.
        n_chars_used = snprintf(left_chars.data, left_chars.size, ",\n");
        if(0 < n_chars_used)
        {
          // Update the character pointer and characters left in the array.
          left_chars.data += n_chars_used;
          left_chars.size -= n_chars_used;
        }
      }

      if(nullptr != name)
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "\"%s\": {\n", name);
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s\"%s\": {\n", indent_level, " ", name);
        }
      }
      else
      {
        if( 0 == indent_level)
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "{\n");
        }
        else
        {
          n_chars_used = snprintf(left_chars.data, left_chars.size, "%*s{\n", indent_level, " ");
        }
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      left_chars = to_string_message(left_chars, indent_level + 2, true);

      if( 0 == indent_level)
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n}");
      }
      else
      {
        n_chars_used = snprintf(left_chars.data, left_chars.size, "\n%*s}", indent_level, " ");
      }

      if(0 < n_chars_used)
      {
        left_chars.data += n_chars_used;
        left_chars.size -= n_chars_used;
      }

      return left_chars;
    }

#endif // End of MSG_TO_STRING

  private:



      FieldNumber which_message_ = FieldNumber::NOT_SET;
      union message
      {
        message() {}
        ~message() {}
        Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH> config_;
        Error error_;
        MotorDiagnostic diagnostic_;
        CommandReply reply_;
      };
      message message_;

      void init_message(const FieldNumber field_id)
      {
        if(FieldNumber::NOT_SET != which_message_)
        {
          // First delete the old object in the oneof.
          clear_message();
        }

        // C++11 unions only support nontrivial members when you explicitly call the placement new statement.
        switch(field_id)
        {
          case FieldNumber::CONFIG:
            new(&message_.config_) Config<ProsthesisMessage_config_Config_params_ModeParams_classification_ClassificationParams_allowed_movements_REP_LENGTH>;
            break;

          case FieldNumber::ERROR:
            new(&message_.error_) Error;
            break;

          case FieldNumber::DIAGNOSTIC:
            new(&message_.diagnostic_) MotorDiagnostic;
            break;

          case FieldNumber::REPLY:
            new(&message_.reply_) CommandReply;
            break;

          default:
            break;
         }

         which_message_ = field_id;
      }

      void clear_message()
      {
        switch(which_message_)
        {
          case FieldNumber::CONFIG:
            ::EmbeddedProto::destroy_at(&message_.config_);
            break;
          case FieldNumber::ERROR:
            ::EmbeddedProto::destroy_at(&message_.error_);
            break;
          case FieldNumber::DIAGNOSTIC:
            ::EmbeddedProto::destroy_at(&message_.diagnostic_);
            break;
          case FieldNumber::REPLY:
            ::EmbeddedProto::destroy_at(&message_.reply_);
            break;
          default:
            break;
        }
        which_message_ = FieldNumber::NOT_SET;
      }

      ::EmbeddedProto::Error deserialize_message(const FieldNumber field_id,
                                    ::EmbeddedProto::ReadBufferInterface& buffer,
                                    const ::EmbeddedProto::WireFormatter::WireType wire_type)
      {
        ::EmbeddedProto::Error return_value = ::EmbeddedProto::Error::NO_ERRORS;

        if(field_id != which_message_)
        {
          init_message(field_id);
        }

        switch(which_message_)
        {
          case FieldNumber::CONFIG:
            return_value = message_.config_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::ERROR:
            return_value = message_.error_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::DIAGNOSTIC:
            return_value = message_.diagnostic_.deserialize_check_type(buffer, wire_type);
            break;
          case FieldNumber::REPLY:
            return_value = message_.reply_.deserialize_check_type(buffer, wire_type);
            break;
          default:
            break;
        }

        if(::EmbeddedProto::Error::NO_ERRORS != return_value)
        {
          clear_message();
        }
        return return_value;
      }

#ifdef MSG_TO_STRING
      ::EmbeddedProto::string_view to_string_message(::EmbeddedProto::string_view& str, const uint32_t indent_level, const bool first_field) const
      {
        ::EmbeddedProto::string_view left_chars = str;

        switch(which_message_)
        {
          case FieldNumber::CONFIG:
            left_chars = message_.config_.to_string(left_chars, indent_level, CONFIG_NAME, first_field);
            break;
          case FieldNumber::ERROR:
            left_chars = message_.error_.to_string(left_chars, indent_level, ERROR_NAME, first_field);
            break;
          case FieldNumber::DIAGNOSTIC:
            left_chars = message_.diagnostic_.to_string(left_chars, indent_level, DIAGNOSTIC_NAME, first_field);
            break;
          case FieldNumber::REPLY:
            left_chars = message_.reply_.to_string(left_chars, indent_level, REPLY_NAME, first_field);
            break;
          default:
            break;
        }

        return left_chars;
      }

#endif // End of MSG_TO_STRING
};

#endif // PROTOCOL_MESSAGES_H
