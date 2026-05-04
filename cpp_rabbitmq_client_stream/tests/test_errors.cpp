#include "test_runner.hpp"
#include "stream/errors.hpp"
#include <stdexcept>
#include <string>

using namespace stream;

TEST(errors_connection_error_message) {
    ConnectionError e("host unreachable");
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("connection error") != std::string::npos);
    ASSERT_TRUE(msg.find("host unreachable") != std::string::npos);
}

TEST(errors_authentication_error_message_and_code) {
    AuthenticationError e(0x0008, "bad credentials");
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("authentication error") != std::string::npos);
    ASSERT_TRUE(msg.find("0008") != std::string::npos);
    ASSERT_TRUE(msg.find("bad credentials") != std::string::npos);
    ASSERT_EQ(e.response_code, uint16_t(0x0008));
}

TEST(errors_stream_error_message_and_code) {
    StreamError e(0x0002, "does not exist");
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("stream error") != std::string::npos);
    ASSERT_TRUE(msg.find("0002") != std::string::npos);
    ASSERT_EQ(e.response_code, uint16_t(0x0002));
}

TEST(errors_protocol_error_message) {
    ProtocolError e("unexpected end of frame");
    std::string msg = e.what();
    ASSERT_TRUE(msg.find("protocol error") != std::string::npos);
    ASSERT_TRUE(msg.find("unexpected end of frame") != std::string::npos);
}

TEST(errors_types_are_distinct) {
    // Each error type should be catchable independently.
    bool caught_conn = false;
    try { throw ConnectionError("x"); }
    catch (const ConnectionError&) { caught_conn = true; }
    catch (...) {}
    ASSERT_TRUE(caught_conn);

    bool caught_auth = false;
    try { throw AuthenticationError(1, "x"); }
    catch (const AuthenticationError&) { caught_auth = true; }
    catch (...) {}
    ASSERT_TRUE(caught_auth);

    bool caught_stream = false;
    try { throw StreamError(1, "x"); }
    catch (const StreamError&) { caught_stream = true; }
    catch (...) {}
    ASSERT_TRUE(caught_stream);

    bool caught_proto = false;
    try { throw ProtocolError("x"); }
    catch (const ProtocolError&) { caught_proto = true; }
    catch (...) {}
    ASSERT_TRUE(caught_proto);
}

TEST(errors_auth_not_caught_as_stream) {
    bool wrong = false;
    try { throw AuthenticationError(1, "x"); }
    catch (const StreamError&) { wrong = true; }
    catch (...) {}
    ASSERT_FALSE(wrong);
}

TEST(errors_response_codes_distinct) {
    ASSERT_TRUE(RESPONSE_CODE_OK                          != RESPONSE_CODE_AUTH_FAILURE);
    ASSERT_TRUE(RESPONSE_CODE_SASL_CHALLENGE              != RESPONSE_CODE_OK);
    ASSERT_TRUE(RESPONSE_CODE_STREAM_ALREADY_EXISTS       != RESPONSE_CODE_STREAM_DOES_NOT_EXIST);
    ASSERT_TRUE(RESPONSE_CODE_VIRTUAL_HOST_ACCESS_FAILURE != RESPONSE_CODE_OK);
}
