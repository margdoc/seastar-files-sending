#ifndef COMMON_H
#define COMMON_H

#include <seastar/rpc/rpc.hh>

using namespace seastar;

const uint64_t aligned_size = 4096;

struct serializer {};
using rpc_protocol = rpc::protocol<serializer>;


template <typename T, typename Output>
inline
void write_arithmetic_type(Output& out, T v) {
    static_assert(std::is_arithmetic<T>::value, "must be arithmetic type");
    return out.write(reinterpret_cast<const char*>(&v), sizeof(T));
}

template <typename T, typename Input>
inline
T read_arithmetic_type(Input& in) {
    static_assert(std::is_arithmetic<T>::value, "must be arithmetic type");
    T v;
    in.read(reinterpret_cast<char*>(&v), sizeof(T));
    return v;
}

template <typename Output>
inline void write(serializer, Output& output, int32_t v) { return write_arithmetic_type(output, v); }
template <typename Output>
inline void write(serializer, Output& output, uint32_t v) { return write_arithmetic_type(output, v); }
template <typename Output>
inline void write(serializer, Output& output, int64_t v) { return write_arithmetic_type(output, v); }
template <typename Output>
inline void write(serializer, Output& output, uint64_t v) { return write_arithmetic_type(output, v); }
template <typename Output>
inline void write(serializer, Output& output, double v) { return write_arithmetic_type(output, v); }
template <typename Input>
inline int32_t read(serializer, Input& input, rpc::type<int32_t>) { return read_arithmetic_type<int32_t>(input); }
template <typename Input>
inline uint32_t read(serializer, Input& input, rpc::type<uint32_t>) { return read_arithmetic_type<uint32_t>(input); }
template <typename Input>
inline uint64_t read(serializer, Input& input, rpc::type<uint64_t>) { return read_arithmetic_type<uint64_t>(input); }
template <typename Input>
inline uint64_t read(serializer, Input& input, rpc::type<int64_t>) { return read_arithmetic_type<int64_t>(input); }
template <typename Input>
inline double read(serializer, Input& input, rpc::type<double>) { return read_arithmetic_type<double>(input); }

template <typename Output>
inline void write(serializer, Output& out, const std::string& v) {
    write_arithmetic_type(out, uint32_t(v.size()));
    out.write(v.c_str(), v.size());
}

template <typename Input>
inline std::string read(serializer, Input& in, rpc::type<std::string>) {
    auto size = read_arithmetic_type<uint32_t>(in);
    std::string ret = uninitialized_string(size);
    in.read(ret.data(), size);
    return ret;
}

using payload_t = std::vector<uint64_t>;

template <typename Output>
inline void write(serializer, Output& out, const payload_t& v) {
    write_arithmetic_type(out, uint32_t(v.size()));
    out.write((const char*)v.data(), v.size() * sizeof(payload_t::value_type));
}

template <typename Input>
inline payload_t read(serializer, Input& in, rpc::type<payload_t>) {
    auto size = read_arithmetic_type<uint32_t>(in);
    payload_t ret;
    ret.resize(size);
    in.read((char*)ret.data(), size * sizeof(payload_t::value_type));
    return ret;
}

#endif // COMMON_H