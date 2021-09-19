#pragma once
#include "JSUtils.h"
#include <type_traits>



template<typename T>
struct ErrorResponse
{
private:

	static_assert(std::is_enum<T>::value, "Must be an enum type");


	std::error_code errcode;
	std::string errorDescription = "";
public:

	T value() const
	{
		return static_cast<T>(errcode.value());
	}


	std::string message() const
	{
		return errcode.message();
	}

	std::string category() const
	{
		return errcode.category().name();
	}

	std::string description() const
	{
		return errorDescription;
	}


	ErrorResponse() {
		errcode = T::OK;
		errorDescription = message();
	}
	ErrorResponse(const T ec, const std::string& description)
	{
		errcode = ec;
		errorDescription = description;
	}

	ErrorResponse(const T ec)
	{
		errcode = ec;
		errorDescription = message();
	}


	ErrorResponse(const nlohmann::json& j)
	{
		fromJson(j);
	}



	nlohmann::json toJson() const
	{
		auto j = nlohmann::json::object();
		j["errorCode"] = value();
		j["errorCategory"] = category();
		j["errorCodeTxt"] = message();
		j["errorDescription"] = errorDescription;
		return j;
	}


	void fromJson(const nlohmann::json& j)
	{
		T errorCode;
		GetJSValueByName(j, "errorCode", "local", errorCode);


		errcode = errorCode;

		std::string errorCategory;

		GetJSValueByName(j, "errorCategory", "local", errorCategory);

		if (errorCategory != errcode.category().name())
		{
			throw std::exception("Unexpected category");
		}

		GetJSValue(j, (*this), errorDescription);
	}

	static nlohmann::json MakeError(T errcode, const std::string& message)
	{
		ErrorResponse<T> e(errcode, message);
		return e.toJson();
	}

	explicit operator bool() const noexcept { // test for actual error
		return (errcode) ? true : false;
	}

	operator std::error_code() const
	{
		return errcode;
	}
};



template<typename T, typename V>
struct ErrResOptional
{
private:
	ErrorResponse<T> error;
	V value;
public:
	ErrResOptional(ErrorResponse<T>&& err)
	{
		error = std::move(err);
		value = {};
	}

	ErrResOptional(T err)
	{
		error = ErrorResponse<T>(err);
	}
	ErrResOptional(const V & Value)
	{
		error = ErrorResponse<T>();
		value = Value;
	}

	ErrResOptional(V&& Value)
	{
		error = ErrorResponse<T>();
		value = std::move(Value);
	}

	operator nlohmann::json() const
	{
		if (error) return error.toJson();
		return value;
	}


	//explicit operator std::string() const
	//{
	//	nlohmann::json j = *this;
	//	return j.dump();
	//}
};

template<typename T, typename V>
inline void to_json(nlohmann::json& j, const ErrResOptional<T,V>& model)
{
	j = model;
}