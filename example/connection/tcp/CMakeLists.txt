# TCP Examples CMakeLists.txt NOTE: Examples temporarily disabled - need major
# API update TcpClient API changes: - connect() returns expected<void,
# system_error> - receive() returns expected<vector<char>, system_error> -
# Options struct field names changed TODO: Rewrite examples to match current API
message(STATUS "TCP examples disabled - pending major API rewrite")
