/*
	zurl.h
	Copyright 2021, zhiayang

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	Version 0.1.0
	=============



	Documentation
	=============

	This library requires `znet.h` and `zbuf.h`.

	This is a library for making HTTP/1.1 requests. It currently supports GET, PUT, POST, and PATCH, but it should
	be trivial to support the rest. You can attach arbitrary HTTP headers and query parameters (eg. ?asdf=A&bsdf=B),
	and SSL is supported as well (through znet).

	Two API styles are supported, for now:
	1. Fully synchronous
		a call to any of the request methods blocks until the entire response is received. The headers and response
		body are returned together.

	2. Callback + synchronous
		a call to any of the request methods blocks until the entire response is received, and only the headers
		are returned. When new data is received, a user-provided callback is called to process the new data. Note
		that the call *still blocks* until the entire response is received.

	Currently it also supports redirect (301) following up to a configurable depth. It is a recursive implementation,
	so the limit should probably be reasonably low.

	Note that if redirect following is enabled, it is *very likely* that on the "real" internet, you will be redirected
	from a HTTP site to a HTTPS site, so you should enable OpenSSL support in znet by defining `ZNET_ENABLE_SSL` to be
	1. A runtime error (`abort()`) will occur if znet attempts to connect to a HTTPS site without SSL enabled.



	Synchronous Callback
	--------------------
	User-provided callbacks to the callback+synchronous API functions should have this signature:
	void callback(int id, zbuf::Span span, std::optional<size_t> total)

	1. id -- this parameter is incremented every time a redirect is followed. since the content buffer is maintained
		by the user, a "typical" implementation would want to clear its buffer when this number changes to remove the
		old content (eg. the 301 HTML status page)

	2. span -- the content. There are no guarantees on the size of this span; a "typical" implementation should probably
		copy this data to another persistent buffer, since the internal buffer (which the span refers to) will be reused
		for subsequent reads.

	3. total -- optionally, the total size of the content. This field is provided if and only if a Content-Length header
		was present in the response HTTP headers. If not, then it is an empty optional. This can be useful for providing
		some sort of progress indication for large request bodies.



	Version History
	===============

	0.1.0 - 15/03/2021
	------------------
	Initial release.
*/


#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdarg>

#include <vector>
#include <string>
#include <optional>
#include <string_view>

#include "zbuf.h"
#include "znet.h"

namespace zurl
{
	struct URL
	{
		explicit URL(zbuf::str_view url);
		URL(zbuf::str_view hostname, uint16_t port);

		const std::string& protocol() const     { return this->m_protocol; }
		const std::string& hostname() const     { return this->m_hostname; }
		const std::string& parameters() const   { return this->m_parameters; }
		const std::string& resource() const     { return this->m_resource; }
		uint16_t port() const                   { return this->m_port; }

		std::string str() const;

	private:
		std::string m_protocol;
		std::string m_hostname;
		std::string m_resource;
		std::string m_parameters;
		uint16_t m_port = 0;
	};


	struct HttpHeaders
	{
		HttpHeaders() { }
		HttpHeaders(zbuf::str_view status);

		HttpHeaders& add(std::string&& key, std::string&& value);
		HttpHeaders& add(const std::string& key, const std::string& value);

		std::string bytes() const;
		std::string status() const;
		int statusCode() const;
		const std::vector<std::pair<std::string, std::string>>& headers() const;

		static std::optional<HttpHeaders> parse(zbuf::str_view data);
		static std::optional<HttpHeaders> parse(const zbuf::Buffer& data);

		std::string get(zbuf::str_view key) const;

	private:
		size_t expected_len = 0;

		std::string m_status;
		std::vector<std::pair<std::string, std::string>> m_headers;
	};

	struct Param
	{
		Param() { }
		Param(std::string name, std::string value) : name(std::move(name)), value(std::move(value)) { }

		std::string name;
		std::string value;
	};

	struct Header
	{
		Header() { }
		Header(std::string name, std::string value) : name(std::move(name)), value(std::move(value)) { }

		std::string name;
		std::string value;
	};

	struct Request
	{
		URL url;

		double timeout = 0;
		int maxRedirects = 8;
		bool followRedirects = false;

		std::vector<Header> headers;
		std::vector<Param> params;

		std::string contentType;
		std::string body;

		int _numRedirects = 0;
	};

	struct Response
	{
		HttpHeaders headers;
		zbuf::Buffer content;
	};

	// synchronous API
	std::optional<Response> get(const Request& request);
	std::optional<Response> put(const Request& request);
	std::optional<Response> post(const Request& request);
	std::optional<Response> patch(const Request& request);

	using RequestCallbackFn = std::function<void (int id, zbuf::Span, std::optional<size_t>)>;

	// asynchronous API
	std::optional<HttpHeaders> get(const Request& request, const RequestCallbackFn& callback);
	std::optional<HttpHeaders> put(const Request& request, const RequestCallbackFn& callback);
	std::optional<HttpHeaders> post(const Request& request, const RequestCallbackFn& callback);
	std::optional<HttpHeaders> patch(const Request& request, const RequestCallbackFn& callback);

	namespace detail
	{
		std::string urlencode(zbuf::str_view s);
		std::string lowercase(zbuf::str_view s);
		std::optional<int64_t> stoi(zbuf::str_view s, int base = 10);
		std::vector<zbuf::str_view> split(zbuf::str_view view, char delim);

		std::string encode_params(const std::vector<Param>& params);


		std::optional<Response> make_http_request(const std::string& method, const Request& request);
		std::optional<HttpHeaders> make_http_request(const std::string& method, const Request& request,
			const RequestCallbackFn& callback);

		// warning: you must free the buffer!!!
		zbuf::str_view vsprint(const char* fmt, ...);
	}
}







#if defined(ZURL_IMPLEMENTATION)

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <unordered_map>

namespace zurl
{
	URL::URL(zbuf::str_view url)
	{
		static std::unordered_map<std::string, uint16_t> default_ports = {
			{ "http", 80 },
			{ "https", 443 },

			{ "ws", 80 },
			{ "wss", 443 },
		};

		do {
			auto i = url.find("://");
			if(i == std::string::npos || i == 0)
				break;

			this->m_protocol = url.take(i).str();
			url.remove_prefix(i + 3);

			// you don't need to have a slash, but if you do it can't be the first thing.
			i = url.find_first_of("?/");
			if(i == 0)
				break;

			auto tmp = url.take(i);
			if(i != (size_t) -1)
				url.remove_prefix(i);   // include the leading / for the path

			this->m_resource = url.str();
			if(i == (size_t) -1 || this->m_resource.empty())
				this->m_resource = "/";

			// need to check for ports. (if i = 0, then your hostname was empty, which is bogus)
			i = tmp.find(':');
			if(i == 0)
				break;

			if(i != std::string::npos)
			{
				// tmp only contains 'basename:PORT'
				auto val = detail::stoi(tmp.drop(i + 1));
				if(!val) break;

				this->m_port = val.value();
				this->m_hostname = tmp.take(i).str();
			}
			else
			{
				this->m_hostname = tmp.str();
				this->m_port = default_ports[this->m_protocol];
			}

			if(auto tmp = this->m_resource.find('?'); tmp != (size_t) -1)
			{
				this->m_resource = this->m_resource.substr(0, tmp);
				if(this->m_resource.empty())
					this->m_resource = "/";

				this->m_parameters = url.drop(tmp + 1).str();
			}

			// ok, success. return to skip the error message.
			return;
		} while(false);

		// lg::error("url", "invalid url '{}'", url);
		fprintf(stderr, "invalid url '%.*s'", (int) url.size(), url.data());
	}

	URL::URL(zbuf::str_view hostname, uint16_t port)
	{
		this->m_hostname = hostname.str();
		this->m_port = port;

		// probably, but it's not important
		this->m_protocol = "http";
	}

	std::string URL::str() const
	{
		char buf[1024] = { };
		auto len = snprintf(buf, 1024, "%s://%s:%d%s",
			this->m_protocol.c_str(),
			this->m_hostname.c_str(),
			(int) this->m_port,
			this->m_resource.c_str());

		// here, we assume that the buffer will not overflow!!!!
		assert(len < 1024);
		return std::string(buf, len);
	}


	HttpHeaders::HttpHeaders(zbuf::str_view status)
	{
		this->m_status = status.str();
		this->expected_len = this->m_status.size() + 2;
	}

	HttpHeaders& HttpHeaders::add(const std::string& key, const std::string& value)
	{
		this->expected_len += 4 + key.size() + value.size();
		this->m_headers.emplace_back(key, value);

		return *this;
	}

	HttpHeaders& HttpHeaders::add(std::string&& key, std::string&& value)
	{
		this->expected_len += 4 + key.size() + value.size();
		this->m_headers.emplace_back(std::move(key), std::move(value));

		return *this;
	}

	std::string HttpHeaders::bytes() const
	{
		std::string ret;
		ret.reserve(this->expected_len + 2);

		ret += this->m_status;
		ret += "\r\n";

		for(auto& [ k, v] : this->m_headers)
			ret += k, ret += ": ", ret += v, ret += "\r\n";

		ret += "\r\n";
		return ret;
	}

	std::string HttpHeaders::status() const
	{
		return this->m_status;
	}

	int HttpHeaders::statusCode() const
	{
		if(this->m_status.empty())
			return 0;

		auto xs = detail::split(this->m_status, ' ');
		if(xs.size() < 3)
			return 0;

		// http version <space> code <space> message
		return (int) detail::stoi(xs[1]).value();
	}

	const std::vector<std::pair<std::string, std::string>>& HttpHeaders::headers() const
	{
		return this->m_headers;
	}

	std::string HttpHeaders::get(zbuf::str_view key) const
	{
		for(const auto& [ k, v ] : this->m_headers)
		{
			if(k == key)
				return v;
		}

		return "";
	}

	std::optional<HttpHeaders> HttpHeaders::parse(const zbuf::Buffer& buf)
	{
		return parse(buf.sv());
	}


	std::optional<HttpHeaders> HttpHeaders::parse(zbuf::str_view data)
	{
		auto x = data.find("\r\n");
		if(x == std::string::npos)
			return std::nullopt;

		auto hdrs = HttpHeaders(data.take(x));
		data.remove_prefix(x + 2);

		while(data.find("\r\n") > 0)
		{
			auto ki = data.find(':');
			if(ki == std::string::npos)
				return std::nullopt;

			auto key = detail::lowercase(data.take(ki));
			data.remove_prefix(ki + 1);

			// strip spaces
			while(data.size() > 0 && data[0] == ' ')
				data.remove_prefix(1);

			if(data.size() == 0)
				return std::nullopt;

			auto vi = data.find("\r\n");
			if(vi == std::string::npos)
				return std::nullopt;

			hdrs.add(std::move(key), data.take(vi).str());

			data.remove_prefix(vi + 2);
		}

		if(data.find("\r\n") != 0)
			return std::nullopt;

		return hdrs;
	}





	std::optional<HttpHeaders> get(const Request& request, const RequestCallbackFn& callback)
	{
		return detail::make_http_request("GET", request, callback);
	}

	std::optional<HttpHeaders> put(const Request& request, const RequestCallbackFn& callback)
	{
		return detail::make_http_request("PUT", request, callback);
	}

	std::optional<HttpHeaders> post(const Request& request, const RequestCallbackFn& callback)
	{
		return detail::make_http_request("POST", request, callback);
	}

	std::optional<HttpHeaders> patch(const Request& request, const RequestCallbackFn& callback)
	{
		return detail::make_http_request("PATCH", request, callback);
	}

	std::optional<Response> get(const Request& request)
	{
		return detail::make_http_request("GET", request);
	}

	std::optional<Response> post(const Request& request)
	{
		return detail::make_http_request("POST", request);
	}

	std::optional<Response> put(const Request& request)
	{
		return detail::make_http_request("PUT", request);
	}

	std::optional<Response> patch(const Request& request)
	{
		return detail::make_http_request("PATCH", request);
	}












	namespace detail
	{
		zbuf::str_view vsprint(const char* fmt, ...)
		{
			va_list ap;
			va_start(ap, fmt);

			va_list ap2;
			va_copy(ap2, ap);

			auto need = vsnprintf(NULL, 0, fmt, ap);
			auto buf = new char[1 + need];

			vsnprintf(buf, 1 + need, fmt, ap2);

			va_end(ap);
			va_end(ap2);

			return zbuf::str_view(buf, need);
		}

		std::optional<int64_t> stoi(zbuf::str_view s, int base)
		{
			if(s.empty())
				return { };

			char* tmp = 0;
			auto ss = s.str();
			int64_t ret = strtoll(ss.c_str(), &tmp, base);
			if(tmp != ss.data() + ss.size())
				return { };

			return ret;
		}

		std::vector<zbuf::str_view> split(zbuf::str_view view, char delim)
		{
			std::vector<zbuf::str_view> ret;

			while(true)
			{
				size_t ln = view.find(delim);

				if(ln != static_cast<size_t>(-1))
				{
					ret.emplace_back(view.data(), ln);
					view.remove_prefix(ln + 1);
				}
				else
				{
					break;
				}
			}

			// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
			if(!view.empty())
				ret.emplace_back(view.data(), view.size());

			return ret;
		}

		std::string lowercase(zbuf::str_view s)
		{
			std::string ret; ret.reserve(s.size());
			for(char c : s)
			{
				if('A' <= c && c <= 'Z')
					ret += (char) (c | 0x20);
				else
					ret += c;
			}
			return ret;
		}

		std::string urlencode(zbuf::str_view s)
		{
			std::string ret; ret.reserve(s.size());
			for(char c : s)
			{
				if(('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '-' || c == '.' || c == '_')
					ret += c;

				else
				{
					char buf[8] = { };
					snprintf(buf, 7, "%%%02x", (uint8_t) c);
					ret += buf;
				}
			}

			return ret;
		}

		std::string encode_params(const std::vector<Param>& params)
		{
			std::string ret;

			if(params.size() > 0)
			{
				ret += "?";
				for(const auto& p : params)
				{
					char buf[1024] = { };
					snprintf(buf, 1024, "%s=%s&", urlencode(p.name).c_str(), urlencode(p.value).c_str());
					ret += buf;
				}

				assert(ret.back() == '&');
				ret.pop_back();
			}

			return ret;
		}


		template <typename Cb>
		std::optional<HttpHeaders> read_response(znet::TCPSocket& sock, double timeout, Cb&& callback)
		{
			constexpr size_t CHUNK_BUFFER_SIZE = 4096;

			size_t processed = 0;

			auto hdrbuf = zbuf::Buffer(CHUNK_BUFFER_SIZE);
			auto chunkbuf = zbuf::Buffer(CHUNK_BUFFER_SIZE);

			bool isChunked = false;
			std::optional<HttpHeaders> headers;
			std::optional<size_t> contentLength;

			auto read_some = [timeout](znet::TCPSocket& sock, zbuf::Buffer& buf) -> bool {
				if(buf.remaining() < CHUNK_BUFFER_SIZE)
					buf.grow(CHUNK_BUFFER_SIZE);

				auto amt = sock.receive(buf.data() + buf.size(), buf.remaining(), timeout);
				if(amt < 0) { fprintf(stderr, "socket error: %s\n", strerror(errno)); return false; }

				buf.incrementSize(amt);
				return true;
			};

			while(true)
			{
				if(!headers.has_value())
				{
					if(!read_some(sock, hdrbuf))
						return { };

					if(auto hdrs = HttpHeaders::parse(hdrbuf); hdrs.has_value())
					{
						headers = hdrs;

						if(auto len = headers->get("content-length"); !contentLength && !len.empty())
							contentLength = static_cast<size_t>(detail::stoi(len).value());

						else if(headers->get("transfer-encoding").find("chunked") != static_cast<size_t>(-1))
							isChunked = true;

						auto tmp = hdrbuf.sv();
						auto i = tmp.find("\r\n\r\n");
						if(i != static_cast<size_t>(-1))
						{
							auto x = tmp.drop(i + 4).span();
							chunkbuf.autoWrite(x.data(), x.size());

							if(!isChunked)
							{
								callback(x, contentLength);
								processed += chunkbuf.size();
							}

							goto have_header;
						}
					}
				}
				else
				{
				have_header:

					if(!isChunked)
					{
						if(contentLength && processed >= *contentLength)
							break;

						// since we are not chunked, then we would have already called the callback with
						// the partial data after the header; thus, we are safe to clear the buffer.
						chunkbuf.unsafeClear();
						if(!read_some(sock, chunkbuf))
							return { };

						processed += chunkbuf.size();
						callback(chunkbuf.span(), contentLength);
					}
					else
					{
						// we might end up here with some data in the chunked buffer already
						if(chunkbuf.sv().find("\r\n") == static_cast<size_t>(-1))
						{
							if(!read_some(sock, chunkbuf))
								return { };

							// from the top.
							continue;
						}

						// because we are constantly shifting the buffer "forward" for each parsed chunk,
						// we know that the start of the buffer must coincide with the start of a chunk.
						auto chunk = chunkbuf.span();
						auto body = chunk.drop(chunk.sv().find("\r\n") + 2);

						// chunks are also apparently terminated by \r\n. so if it's not there,
						// we don't have the whole chunk yet, so bail for now.
						if(body.sv().rfind("\r\n") + 2 != body.size())
							continue;

						body = body.drop_last(2);

						// ok, we have a complete body now.
						// first line contains the chunk size, as well as other nonsense.
						auto tmp2 = chunk.take(chunk.sv().find("\r\n")).take(chunk.sv().find(';'));
						auto size = static_cast<size_t>(detail::stoi(tmp2.sv(), /* base: */ 16).value());

						// if the size of the body and the indicate size don't match, then we also have a problem
						if(size != body.size())
							fprintf(stderr, "chunk size mismatch: expected %zu, got %zu\n", size, body.size());

						callback(body, contentLength);

						// ok, now shift the data.
						auto todrop = (body.data() + body.size()) - chunkbuf.data();
						chunkbuf.drop(todrop);

						// chunks are "null-terminated". (ie. last chunk has a size of 0)
						if(size == 0)
							break;
					}
				}
			}

			return headers;
		}

		std::optional<Response> make_http_request(const std::string& method, const Request& request)
		{
			int curid = 0;
			auto buf = zbuf::Buffer(512);
			auto hdr = detail::make_http_request(method, request, [&buf, &curid](int id, zbuf::Span s, ...) {
				if(curid != id)
					curid = id, buf.clear();

				buf.autoWrite(s);
			});

			if(!hdr) return { };

			return Response {
				.headers = std::move(hdr.value()),
				.content = std::move(buf)
			};
		}

		std::optional<HttpHeaders> make_http_request(const std::string& method, const Request& request, const RequestCallbackFn& callback)
		{
			auto tmpsv = detail::vsprint("%s://%s", request.url.protocol().c_str(), request.url.hostname().c_str());
			auto address = URL(tmpsv);
			auto path = request.url.resource();

			delete[] tmpsv.data();
			tmpsv.clear();

			// open a socket, write, wait for response, close.
			auto sock = znet::TCPSocket(
				znet::IPAddress::hostname4(request.url.hostname(), request.url.port()),
				/* ssl: */ request.url.protocol() == "https"
			);

			if(!sock.connect(request.timeout))
				return { };

			tmpsv = detail::vsprint("%s %s%s HTTP/1.1", method.c_str(), path.c_str(),
				detail::encode_params(request.params).c_str());

			auto hdr = HttpHeaders(tmpsv);
			delete[] tmpsv.data();


			hdr.add("Host", request.url.hostname());
			for(const auto& h : request.headers)
				hdr.add(h.name, h.value);

			if(request.body.size() > 0)
				hdr.add("Content-Type", request.contentType.empty() ? "text/plain" : request.contentType);

			hdr.add("Content-Length", std::to_string(request.body.size()));

			auto h = hdr.bytes();
			auto buf = zbuf::Buffer(h.size() + request.body.size());

			buf.autoWrite(zbuf::Span::fromString(h));
			buf.autoWrite(zbuf::Span::fromString(request.body));

			sock.send(buf.data(), buf.size());

			// the actual socket reader has no concept of an "id" -- this is purely a request thing.
			// so, we need to wrap it in another lambda to pass in the id.
			auto resp = detail::read_response(sock, request.timeout, [&request, &callback](auto... xs) {
				callback(request._numRedirects, static_cast<decltype(xs)&&>(xs)...);
			});

			if(!resp) return { };

			auto r = std::move(resp.value());
			sock.disconnect();

			if(r.statusCode() == 301)
			{
				if(!request.followRedirects || request._numRedirects > request.maxRedirects)
					goto out;

				auto to = r.get("location");
				if(to.empty())
					goto out;

				auto copy = request;
				copy._numRedirects += 1;
				copy.url = URL(to);

				return make_http_request(method, copy, callback);
			}

		out:
			return r;

			// return Response {
			// 	.headers = std::move(r.first),
			// 	.content = std::move(r.second)
			// };
		}
	}
}


#endif // defined(ZURL_IMPLEMENTATION)
