#include <coroutine>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fmt/ostream.h>
#include <memory>
#include <seastar/core/temporary_buffer.hh>
#include <seastar/net/socket_defs.hh>
#include <string>

#include <seastar/core/app-template.hh>
#include <seastar/core/file.hh>
#include <seastar/core/file-types.hh>
#include <seastar/core/future.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/when_all.hh>
#include <seastar/net/inet_address.hh>
#include <seastar/rpc/rpc.hh>
#include <seastar/core/sleep.hh>

#include "common.hh"

using namespace seastar;

namespace bpo = boost::program_options;

logger slogger{"server"};


int main(int ac, char** av) {
    app_template app;
    app.add_options()
        ("port", bpo::value<uint16_t>()->default_value(10000), "RPC server port")
        ("dir", bpo::value<std::filesystem::path>(), "Uploaded files directory")
    ;

    return app.run(ac, av, [&] () -> future<> {
        auto& opts = app.configuration();
        auto port = opts["port"].as<uint16_t>();
        auto dir = opts["dir"].as<std::filesystem::path>();

        rpc_protocol rpc{serializer{}};

        uint64_t connection_id{0};
        
        rpc.register_handler(1, [dir = std::move(dir), &connection_id] (int aux_data, rpc::source<std::string> source) mutable {
            ++connection_id;

            slogger.debug("[{}] New connection", connection_id);
            rpc::sink<std::string> sink = source.make_sink<serializer, std::string>();

            (void)async([&, sink, source, connection_id] () mutable {
                std::unique_ptr<char[]> write_buffer{new char[aligned_size]},
                                        receive_buffer{new char[aligned_size]};

                auto filename_opt = source().get0();
                assert(filename_opt.has_value());
                auto [filename] = *filename_opt;
                slogger.debug("[{}] Got filename: {}", connection_id, filename);
                
                auto file_path = (dir / filename).string();
                slogger.debug("[{}] Creating: {}", connection_id, file_path);
                auto file = open_file_dma(file_path, open_flags::create | open_flags::rw).get();

                uint64_t bytes_written{0};
                bool eos{false};

                auto receive_chunk = [&] () -> future<> {
                    return async([&] () {
                        auto chunk_opt = source().get();

                        if (chunk_opt.has_value()) {
                            auto [chunk] = *chunk_opt;
                            slogger.debug("[{}] Got {} bytes", connection_id, chunk.size());
                            std::memset(receive_buffer.get(), 0, aligned_size);
                            std::memcpy(receive_buffer.get(), chunk.c_str(), chunk.size());
                        } else {
                            slogger.debug("[{}] End of stream", connection_id);
                            eos = true;
                        }
                    });
                };

                auto write_chunk = [&] () -> future<> {
                    return file.dma_write(bytes_written, write_buffer.get(), aligned_size).then([&] (size_t written) {
                        assert(aligned_size == written);
                        bytes_written += written;
                        slogger.debug("[{}] Written {} bytes (all={})", connection_id, aligned_size, bytes_written);
                    });
                };

                receive_chunk().get();
                std::swap(write_buffer, receive_buffer);
                while (!eos) {
                    slogger.debug("[{}] Loop", connection_id);
                    when_all_succeed(write_chunk(), receive_chunk()).get();
                    std::swap(write_buffer, receive_buffer);
                }

                slogger.debug("[{}] Closing", connection_id);
            }).finally([sink] () mutable {
                return sink.flush();
            }).finally([sink] () mutable {
                return sink.close();
            }).finally([sink] () {});
                
            return sink;
        });


        slogger.debug("Starting server");
        rpc::server_options so;
        so.streaming_domain = rpc::streaming_domain_type(1);
        rpc_protocol::server server{rpc, so, ipv4_addr{"127.0.0.1", port}, {}};

        co_await seastar::sleep_abortable(std::chrono::hours(1));
        co_await server.stop();
    });
}