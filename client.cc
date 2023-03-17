#include <coroutine>
#include <filesystem>
#include <memory>
#include <seastar/rpc/rpc.hh>
#include <seastar/util/log.hh>
#include <string>

#include <seastar/core/app-template.hh>
#include <seastar/core/file.hh>
#include <seastar/core/file-types.hh>
#include <seastar/core/future.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/when_all.hh>
#include <seastar/net/inet_address.hh>
#include <seastar/util/defer.hh>

#include "common.hh"

using namespace seastar;

namespace bpo = boost::program_options;

logger clogger{"client"};


int main(int ac, char** av) {
    app_template app;
    app.add_options()
        ("port", bpo::value<uint16_t>()->default_value(10000), "RPC server port")
        ("server", bpo::value<std::string>(), "Server address")
        ("file", bpo::value<std::filesystem::path>(), "File to send")
    ;

    return app.run(ac, av, [&] {
        return async([&] {
            auto& opts = app.configuration();
            auto port = opts["port"].as<uint16_t>();
            auto& server = opts["server"].as<std::string>();
            auto& file_path = opts["file"].as<std::filesystem::path>();

            clogger.debug("Opening file {}", file_path.string());
            auto file = open_file_dma(file_path.string(), open_flags::ro).get();

            std::unique_ptr<char[]> read_buffer{new char[aligned_size]},
                                    to_send_buffer{new char[aligned_size]};
            uint64_t file_size{file.size().get()},
                     bytes_read{0},
                     read_chunk_size,
                     to_send_chunk_size;

            clogger.debug("File size = {}", file_size);

            auto read_chunk = [&] () -> future<> {
                return file.dma_read<char>(bytes_read, read_buffer.get(), aligned_size).then([&bytes_read, &read_chunk_size] (size_t size) {
                    bytes_read += size;
                    read_chunk_size = size;
                    clogger.debug("Read {} bytes (all={})", read_chunk_size, bytes_read);
                });
            };

            clogger.debug("Initializing connection");
            rpc_protocol rpc{serializer{}};
            rpc_protocol::client client{rpc, rpc::client_options{}, socket_address{net::inet_address::find(server).get(), port}};

            auto client_kill = defer([&client] () {
                clogger.debug("Closing connection");
                client.stop().get();
            });

            auto rpc_call = rpc.make_client<rpc::source<std::string>(int, rpc::sink<std::string>)>(1);
            auto sink = client.make_stream_sink<serializer, std::string>().get();
            auto source = rpc_call(client, 666, sink).get0();

            clogger.debug("Sending filename: {}", file_path.filename().string());
            sink(file_path.filename().string()).get();

            auto send_chunk = [&] () -> future<> {
                return sink(std::string{to_send_buffer.get(), to_send_chunk_size}).then([=] () {
                    clogger.debug("Sent {} bytes", to_send_chunk_size);
                });
            };

            read_chunk().get();
            std::swap(read_buffer, to_send_buffer);
            std::swap(read_chunk_size, to_send_chunk_size);
            while (bytes_read < file_size) {
                clogger.debug("Loop {} {}", bytes_read, file_size);
                when_all_succeed(read_chunk(), send_chunk()).get();
                std::swap(read_buffer, to_send_buffer);
                std::swap(read_chunk_size, to_send_chunk_size);
            }
            send_chunk().get();

            sink.flush().finally([sink] () mutable {
                return sink.close();
            }).get();
            source().get();
            clogger.debug("End");
        });
    });
}