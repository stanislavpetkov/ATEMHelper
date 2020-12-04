#pragma once
#include <string>
#include <vector>

namespace HTTP
{

	const std::string RESOURCES_FILE_PATH = "./RES/";

	namespace methods
	{
		static const std::string s_get_method = "GET";
		static const std::string s_post_method = "POST";
		static const std::string s_put_method = "PUT";
		static const std::string s_patch_method = "PATCH";
		static const std::string s_delete_method = "DELETE";
		static const std::string s_trace_method = "TRACE";
		static const std::string s_copy_method = "COPY";
		static const std::string s_move_method = "MOVE";
		static const std::string s_head_method = "HEAD";
		static const std::string s_options_method = "OPTIONS";
		static const std::string s_link_method = "LINK";
		static const std::string s_unlink_method = "UNLINK";
		static const std::string s_purge_method = "PURGE";
		static const std::string s_lock_method = "LOCK";
		static const std::string s_unlock_method = "UNLOCK";
		static const std::string s_propfind_method = "PROPFIND";
		static const std::string s_proppatch_method = "PROPPATCH";
		static const std::string s_view_method = "VIEW";
		static const std::string s_connect_method = "CONNECT";
		static const std::string s_track_method = "TRACK";
		static const std::string s_mkcol_method = "MKCOL";
		static const std::string s_search_method = "SEARCH";

		



		static std::string access_methods[] = {
			s_get_method,
			s_post_method,
			s_put_method,
			s_patch_method,
			s_delete_method,
			s_copy_method,
			s_head_method,
			s_options_method,
			s_link_method,
			s_unlink_method,
			s_purge_method,
			s_lock_method,
			s_unlock_method,
			s_propfind_method,
			s_view_method
		};
	}


	

	enum class StatusCode :uint32_t {
		unknown = 0,
		information_continue = 100,
		information_switching_protocols,
		information_processing,
		success_ok = 200,
		success_created,
		success_accepted,
		success_non_authoritative_information,
		success_no_content,
		success_reset_content,
		success_partial_content,
		success_multi_status,
		success_already_reported,
		success_im_used = 226,
		redirection_multiple_choices = 300,
		redirection_moved_permanently,
		redirection_found,
		redirection_see_other,
		redirection_not_modified,
		redirection_use_proxy,
		redirection_switch_proxy,
		redirection_temporary_redirect,
		redirection_permanent_redirect,
		client_error_bad_request = 400,
		client_error_unauthorized,
		client_error_payment_required,
		client_error_forbidden,
		client_error_not_found,
		client_error_method_not_allowed,
		client_error_not_acceptable,
		client_error_proxy_authentication_required,
		client_error_request_timeout,
		client_error_conflict,
		client_error_gone,
		client_error_length_required,
		client_error_precondition_failed,
		client_error_payload_too_large,
		client_error_uri_too_long,
		client_error_unsupported_media_type,
		client_error_range_not_satisfiable,
		client_error_expectation_failed,
		client_error_im_a_teapot,
		client_error_misdirection_required = 421,
		client_error_unprocessable_entity,
		client_error_locked,
		client_error_failed_dependency,
		client_error_upgrade_required = 426,
		client_error_precondition_required = 428,
		client_error_too_many_requests,
		client_error_request_header_fields_too_large = 431,
		client_error_unavailable_for_legal_reasons = 451,
		server_error_internal_server_error = 500,
		server_error_not_implemented,
		server_error_bad_gateway,
		server_error_service_unavailable,
		server_error_gateway_timeout,
		server_error_http_version_not_supported,
		server_error_variant_also_negotiates,
		server_error_insufficient_storage,
		server_error_loop_detected,
		server_error_not_extended = 510,
		server_error_network_authentication_required
	};

	const static std::vector<std::pair<StatusCode, std::string>> &status_codes() noexcept {
		const static std::vector<std::pair<StatusCode, std::string>> status_codes = {
			{ StatusCode::unknown, "" },
		{ StatusCode::information_continue, "Continue" },
		{ StatusCode::information_switching_protocols, "Switching Protocols" },
		{ StatusCode::information_processing, "Processing" },
		{ StatusCode::success_ok, "OK" },
		{ StatusCode::success_created, "Created" },
		{ StatusCode::success_accepted, "Accepted" },
		{ StatusCode::success_non_authoritative_information, "Non-Authoritative Information" },
		{ StatusCode::success_no_content, "No Content" },
		{ StatusCode::success_reset_content, "Reset Content" },
		{ StatusCode::success_partial_content, "Partial Content" },
		{ StatusCode::success_multi_status, "Multi-Status" },
		{ StatusCode::success_already_reported, "Already Reported" },
		{ StatusCode::success_im_used, "IM Used" },
		{ StatusCode::redirection_multiple_choices, "Multiple Choices" },
		{ StatusCode::redirection_moved_permanently, "Moved Permanently" },
		{ StatusCode::redirection_found, "Found" },
		{ StatusCode::redirection_see_other, "See Other" },
		{ StatusCode::redirection_not_modified, "Not Modified" },
		{ StatusCode::redirection_use_proxy, "Use Proxy" },
		{ StatusCode::redirection_switch_proxy, "Switch Proxy" },
		{ StatusCode::redirection_temporary_redirect, "Temporary Redirect" },
		{ StatusCode::redirection_permanent_redirect, "Permanent Redirect" },
		{ StatusCode::client_error_bad_request, "Bad Request" },
		{ StatusCode::client_error_unauthorized, "Unauthorized" },
		{ StatusCode::client_error_payment_required, "Payment Required" },
		{ StatusCode::client_error_forbidden, "Forbidden" },
		{ StatusCode::client_error_not_found, "Not Found" },
		{ StatusCode::client_error_method_not_allowed, "Method Not Allowed" },
		{ StatusCode::client_error_not_acceptable, "Not Acceptable" },
		{ StatusCode::client_error_proxy_authentication_required, "Proxy Authentication Required" },
		{ StatusCode::client_error_request_timeout, "Request Timeout" },
		{ StatusCode::client_error_conflict, "Conflict" },
		{ StatusCode::client_error_gone, "Gone" },
		{ StatusCode::client_error_length_required, "Length Required" },
		{ StatusCode::client_error_precondition_failed, "Precondition Failed" },
		{ StatusCode::client_error_payload_too_large, "Payload Too Large" },
		{ StatusCode::client_error_uri_too_long, "URI Too Long" },
		{ StatusCode::client_error_unsupported_media_type, "Unsupported Media Type" },
		{ StatusCode::client_error_range_not_satisfiable, "Range Not Satisfiable" },
		{ StatusCode::client_error_expectation_failed, "Expectation Failed" },
		{ StatusCode::client_error_im_a_teapot, "I'm a teapot" },
		{ StatusCode::client_error_misdirection_required, "Misdirected Request" },
		{ StatusCode::client_error_unprocessable_entity, "Unprocessable Entity" },
		{ StatusCode::client_error_locked, "Locked" },
		{ StatusCode::client_error_failed_dependency, "Failed Dependency" },
		{ StatusCode::client_error_upgrade_required, "Upgrade Required" },
		{ StatusCode::client_error_precondition_required, "Precondition Required" },
		{ StatusCode::client_error_too_many_requests, "Too Many Requests" },
		{ StatusCode::client_error_request_header_fields_too_large, "Request Header Fields Too Large" },
		{ StatusCode::client_error_unavailable_for_legal_reasons, "Unavailable For Legal Reasons" },
		{ StatusCode::server_error_internal_server_error, "Internal Server Error" },
		{ StatusCode::server_error_not_implemented, "Not Implemented" },
		{ StatusCode::server_error_bad_gateway, "Bad Gateway" },
		{ StatusCode::server_error_service_unavailable, "Service Unavailable" },
		{ StatusCode::server_error_gateway_timeout, "Gateway Timeout" },
		{ StatusCode::server_error_http_version_not_supported, "HTTP Version Not Supported" },
		{ StatusCode::server_error_variant_also_negotiates, "Variant Also Negotiates" },
		{ StatusCode::server_error_insufficient_storage, "Insufficient Storage" },
		{ StatusCode::server_error_loop_detected, "Loop Detected" },
		{ StatusCode::server_error_not_extended, "Not Extended" },
		{ StatusCode::server_error_network_authentication_required, "Network Authentication Required" } };
		return status_codes;
	}

	inline StatusCode status_code(const std::string &status_code_str) noexcept {
		for (auto &status_code : status_codes()) {
			if (status_code.second == status_code_str)
				return status_code.first;
		}
		return StatusCode::unknown;
	}

	inline const std::string status_code(StatusCode status_code_enum) noexcept {
		for (auto &status_code : status_codes()) {
			if (status_code.first == status_code_enum)
				return status_code.second;
		}
		return status_codes()[0].second;
	}






	



namespace {

	struct Err_HttpCategory : std::error_category
	{
		const char* name() const noexcept override;
		std::string message(int ev) const override;
	};

	const char* Err_HttpCategory::name() const noexcept
	{
		return "HTTP";
	}

	std::string Err_HttpCategory::message(int ev) const
	{
		return status_code(static_cast<HTTP::StatusCode>(ev));		
	}
}

const Err_HttpCategory theErr_HttpCategory{};

static std::error_code make_error_code(HTTP::StatusCode e)
{
	return { static_cast<int>(e), theErr_HttpCategory };
}





}



namespace std
{
	template <>
	struct is_error_code_enum<HTTP::StatusCode> : true_type {};
}