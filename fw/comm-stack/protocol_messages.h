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

        default:
          break;
      }

    }

    ~DeviceMessage() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      HELLO = 1,
      DATA = 2
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

        default:
          break;
      }

    }

    ~ProsthesisMessage() override = default;

    enum class FieldNumber : uint32_t
    {
      NOT_SET = 0,
      CONFIG = 1,
      ERROR = 2
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
          default:
            break;
        }

        return left_chars;
      }

#endif // End of MSG_TO_STRING
};

#endif // PROTOCOL_MESSAGES_H