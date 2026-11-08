/*******************************************************************************
 * tefkernel - net_engine
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/11/7
 *******************************************************************************/

#include "terraria/content/net_engine.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "tef/patchlib/struct/string.hpp"
#include "abi_impl/tefpackage/lz4.h"

#include "mod_core.hpp"
#include "modloader_core_imp.hpp"
#include "module_core_imp.hpp"
#include "terraria/main_state.hpp"

namespace terraria::content {
    using namespace tef;

    namespace {
        constexpr uint8_t k_proto_version = 0x01;
        constexpr int k_mod_message_id = 6;
        constexpr uint64_t k_version_code = 2026102800ULL;
        constexpr std::string_view k_magic = "Terraria With TEFKernel";

        // ---- 序列化（C++20: span / 小端手写即可，不值得上 endian）----
        constexpr void put_u16(std::vector<uint8_t> &out, const uint16_t v) {
            out.push_back(static_cast<uint8_t>(v & 0xFF));
            out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }

        constexpr void put_u32(std::vector<uint8_t> &out, const uint32_t v) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }

        constexpr void put_str(std::vector<uint8_t> &out, const std::string_view s) {
            out.push_back(static_cast<uint8_t>(s.size() & 0xFF));
            out.insert(out.end(), s.begin(), s.end());
        }

        [[nodiscard]] constexpr uint16_t read_u16(const uint8_t *p) {
            return static_cast<uint16_t>(p[0] | (p[1] << 8));
        }

        [[nodiscard]] constexpr uint32_t read_u32(const uint8_t *p) {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                   (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        }

        // ---- 版本表项（C++20: 用 std::string 替代 char[128]，省 strncpy）----
        struct version_entry {
            std::string id;
            int version = 0;
            bool matched = false;
        };

        struct mod_entry {
            std::string id;
            int version = 0;
            std::string ml_id;
            bool matched = false;
        };

        struct ml_group {
            std::string ml_id;
            std::string errors;
            int count = 0;
        };

        // 10 条上限的错误拼接，用 std::string::append + snprintf
        template<typename... Args>
        void append_fmt(std::string &out, const char *fmt, Args... args) {
            char buf[256];
            if (const int n = std::snprintf(buf, sizeof(buf), fmt, args...); n > 0)
                out.append(
                    buf, static_cast<size_t>(std::min<int>(n, sizeof(buf) - 1)));
        }

        ml_group &group_for(std::vector<ml_group> &groups, const std::string_view ml_id) {
            // C++20: ranges::find_if
            const auto it = std::ranges::find_if(groups,
                                                 [&](const ml_group &g) { return g.ml_id == ml_id; });
            if (it != groups.end()) return *it;
            groups.push_back({.ml_id = std::string(ml_id), .errors = {}, .count = 0});
            return groups.back();
        }

        // 通用比较：每个 entry 在 remote 里找同 id 的，匹配/不匹配/缺失/多余
        struct compare_stat {
            int errors = 0;
            std::string text;
        };

        compare_stat compare_version_lists(const std::vector<version_entry> &local,
                                           std::vector<version_entry> &remote) {
            compare_stat st;
            if (local.size() != remote.size()) {
                append_fmt(st.text, "count: %zu vs %zu\n", local.size(), remote.size());
                st.errors++;
            }
            for (auto &l: local) {
                auto it = std::ranges::find_if(remote,
                                               [&](const version_entry &r) { return r.id == l.id; });
                if (it == remote.end()) {
                    if (st.errors < 10) {
                        append_fmt(st.text, "%s : missing (v%d)\n", l.id.c_str(), l.version);
                        st.errors++;
                    }
                } else {
                    it->matched = true;
                    if (it->version != l.version && st.errors < 10) {
                        append_fmt(st.text, "%s : v%d != v%d\n", l.id.c_str(), l.version, it->version);
                        st.errors++;
                    }
                }
            }
            for (auto &[id, version, matched]: remote)
                if (!matched && st.errors < 10) {
                    append_fmt(st.text, "%s : extra (v%d)\n", id.c_str(), version);
                    st.errors++;
                }
            return st;
        }

#if defined(__ANDROID__)
        void (*g_orig_process_data)(void *, void *, int, int *) = nullptr;
        void (*g_orig_send_data)(int, int, int, void *, int, float, float, float,
                                 int, int, int) = nullptr;
        bool (*g_orig_client_send)(void *, void *, int) = nullptr;

        void process_data_detour(void *self, void *data, int length, int *mt) {
            void *args[2] = {&data, &length};
            net_engine::dispatch_process_data(self, args);
            if (g_orig_process_data) g_orig_process_data(self, data, length, mt);
        }
        void send_data_detour(int msg, int rc, int ic, void *text, int number,
                              float n2, float n3, float n4, int n5, int n6, int n7) {
            void *args[11] = {&msg, &rc, &ic, &text, &number, &n2, &n3, &n4, &n5, &n6, &n7};
            net_engine::dispatch_send_data(args);
            if (g_orig_send_data)
                g_orig_send_data(msg, rc, ic, text, number, n2, n3, n4, n5, n6, n7);
        }
        bool client_send_detour(void *self, void *data, int length) {
            void *args[2] = {&data, &length};
            net_engine::dispatch_client_send(args);
            return g_orig_client_send ? g_orig_client_send(self, data, length) : false;
        }
#endif
    } // namespace

    net_engine &net_engine::instance() noexcept {
        static net_engine inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 版本序列化
    // -----------------------------------------------------------------------

    bool net_engine::get_all_versions(std::vector<uint8_t> &out) {
        out.clear();
        out.reserve(4096);
        out.push_back(k_proto_version);

        // Module
        {
            auto &mods = module::module_registry::instance().handles();
            put_u16(out, static_cast<uint16_t>(mods.size()));
            for (auto &m: mods) {
                auto *info = module::module_registry::get_info(m.get());
                if (!info) continue;
                put_str(out, info->pkg_id ? info->pkg_id : "");
                put_u32(out, static_cast<uint32_t>(info->version_code));
            }
        }
        // ModLoader
        {
            auto &mls = ml::mod_loader_registry::instance().handles();
            put_u16(out, static_cast<uint16_t>(mls.size()));
            for (auto &m: mls) {
                auto *info = ml::mod_loader_registry::get_info(m.get());
                if (!info) continue;
                put_str(out, info->pkg_id ? info->pkg_id : "");
                put_u32(out, static_cast<uint32_t>(info->version_code));
            }
        }
        // Mod
        {
            auto &list = mod::mod_registry::instance().handles();
            put_u16(out, static_cast<uint16_t>(list.size()));
            for (auto &h: list) {
                const auto *m = h.get();
                if (!m || !m->manifest || m->mod_id.empty() || !m->owner_ml) continue;
                put_str(out, m->mod_id);
                const auto *mp = m->owner_ml->ml_entry->ops->get_multiplayer_info(m->manifest);
                put_u32(out, static_cast<uint32_t>(mp ? mp->version_code : 0));
            }
        }

        // LZ4
        std::vector<uint8_t> compressed(static_cast<size_t>(LZ4_compressBound(static_cast<int>(out.size()))) + 4);
        const auto orig = static_cast<uint32_t>(out.size());
        compressed[0] = static_cast<uint8_t>((orig >> 24) & 0xFF);
        compressed[1] = static_cast<uint8_t>((orig >> 16) & 0xFF);
        compressed[2] = static_cast<uint8_t>((orig >> 8) & 0xFF);
        compressed[3] = static_cast<uint8_t>(orig & 0xFF);
        const int csize = LZ4_compress_default(
            reinterpret_cast<const char *>(out.data()),
            reinterpret_cast<char *>(compressed.data() + 4),
            static_cast<int>(out.size()), static_cast<int>(compressed.size() - 4));
        if (csize <= 0) {
            log::error("net: LZ4 compress failed");
            return false;
        }
        compressed.resize(4 + static_cast<size_t>(csize));
        out = std::move(compressed);
        log::info("net: serialized {} -> {} bytes", orig, csize);
        return true;
    }

    // -----------------------------------------------------------------------
    // 版本比较
    // -----------------------------------------------------------------------

    bool net_engine::compare_versions(const uint8_t *remote, uint32_t remote_size,
                                      char *error_msg, size_t error_msg_size) {
        if (!remote || remote_size < 4) {
            std::snprintf(error_msg, error_msg_size, "Invalid data");
            return false;
        }
        const uint32_t orig = read_u32(remote);
        if (orig > 1024 * 1024) {
            std::snprintf(error_msg, error_msg_size, "Data too large: %u", orig);
            return false;
        }

        std::vector<uint8_t> dec(orig);
        const int dsize = LZ4_decompress_safe_partial(
            reinterpret_cast<const char *>(remote + 4),
            reinterpret_cast<char *>(dec.data()),
            static_cast<int>(remote_size - 4), static_cast<int>(orig), static_cast<int>(orig));
        if (dsize < 0) {
            std::snprintf(error_msg, error_msg_size,
                          "LZ4 failed: ret=%d orig_size=%u remote_size=%u", dsize, orig, remote_size);
            return false;
        }

        auto payload = std::span<const uint8_t>(dec.data(), static_cast<size_t>(dsize));
        size_t off = 0;
        if (payload.empty()) {
            std::snprintf(error_msg, error_msg_size, "Empty payload");
            return false;
        }
        if (const uint8_t proto = payload[off++]; proto != k_proto_version) {
            std::snprintf(error_msg, error_msg_size,
                          "Protocol mismatch: local=%u, remote=%u", k_proto_version, proto);
            return false;
        }

        auto read_count = [&]() -> std::optional<uint16_t> {
            if (off + 2 > payload.size()) return std::nullopt;
            const uint16_t v = read_u16(payload.data() + off);
            off += 2;
            return v;
        };
        auto read_entry = [&](std::string &id) -> std::optional<int> {
            if (off + 1 > payload.size()) return std::nullopt;
            const uint8_t n = payload[off++];
            if (off + n + 4 > payload.size()) return std::nullopt;
            id.assign(reinterpret_cast<const char *>(payload.data() + off), n);
            off += n;
            const int version = static_cast<int>(read_u32(payload.data() + off));
            off += 4;
            return version;
        };

        int total_errors = 0;
        std::string module_errors;

        // Module
        {
            std::vector<version_entry> rmod, lmod;
            auto rc = read_count();
            if (!rc) {
                std::snprintf(error_msg, error_msg_size, "Truncated module count");
                return false;
            }
            for (auto &m: module::module_registry::instance().handles()) {
                auto *info = module::module_registry::get_info(m.get());
                if (!info) continue;
                lmod.push_back(
                    {.id = info->pkg_id ? info->pkg_id : "", .version = info->version_code, .matched = false});
            }
            for (uint16_t i = 0; i < *rc; ++i) {
                version_entry e{};
                auto v = read_entry(e.id);
                if (!v) break;
                e.version = *v;
                rmod.push_back(std::move(e));
            }
            auto [errors, text] = compare_version_lists(lmod, rmod);
            module_errors = std::move(text);
            total_errors += errors;
        }

        // ModLoader + Mod
        if (auto rc = read_count()) {
            std::string ml_errors;
            std::vector<version_entry> lml, rml;
            for (auto &m: ml::mod_loader_registry::instance().handles()) {
                auto *info = ml::mod_loader_registry::get_info(m.get());
                if (!info) continue;
                lml.push_back({
                    .id = info->pkg_id ? info->pkg_id : "", .version = info->version_code, .matched = false
                });
            }
            for (uint16_t i = 0; i < *rc; ++i) {
                version_entry e{};
                auto v = read_entry(e.id);
                if (!v) break;
                e.version = *v;
                rml.push_back(std::move(e));
            }
            auto [errors, text] = compare_version_lists(lml, rml);
            ml_errors = std::move(text);
            total_errors += errors;

            // Mod
            if (auto rmc = read_count()) {
                std::vector<ml_group> groups;
                std::vector<mod_entry> lm, rm;
                for (auto &h: mod::mod_registry::instance().handles()) {
                    auto *m = h.get();
                    if (!m || !m->manifest || m->mod_id.empty() || !m->owner_ml) continue;
                    mod_entry e{};
                    e.id = m->mod_id;
                    const auto *mp = m->owner_ml->ml_entry->ops->get_multiplayer_info(m->manifest);
                    e.version = mp ? mp->version_code : 0;
                    e.ml_id = m->owner_ml->ml_entry->info ? m->owner_ml->ml_entry->info->pkg_id : "unknown";
                    lm.push_back(std::move(e));
                }
                for (uint16_t i = 0; i < *rmc; ++i) {
                    mod_entry e{};
                    std::string id;
                    auto v = read_entry(id);
                    if (!v) break;
                    e.id = std::move(id);
                    e.version = *v;
                    e.ml_id = "?";
                    rm.push_back(std::move(e));
                }
                for (auto &l: lm) {
                    auto it = std::ranges::find_if(rm,
                                                   [&](const mod_entry &r) { return r.id == l.id; });
                    if (it == rm.end()) {
                        if (auto &g = group_for(groups, l.ml_id); g.count < 10) {
                            append_fmt(g.errors, "%s : missing (v%d)\n", l.id.c_str(), l.version);
                            g.count++;
                            total_errors++;
                        }
                    } else {
                        it->matched = true;
                        if (it->version != l.version) {
                            if (auto &g = group_for(groups, l.ml_id); g.count < 10) {
                                append_fmt(g.errors, "%s : v%d != v%d\n",
                                           l.id.c_str(), l.version, it->version);
                                g.count++;
                                total_errors++;
                            }
                        }
                    }
                }
                for (auto &r: rm)
                    if (!r.matched) {
                        if (auto &g = group_for(groups, "?"); g.count < 10) {
                            append_fmt(g.errors, "%s : extra (v%d)\n", r.id.c_str(), r.version);
                            g.count++;
                            total_errors++;
                        }
                    }

                if (total_errors > 0) {
                    std::string details;
                    append_fmt(details, "%d error(s)\n", total_errors);
                    if (!module_errors.empty()) {
                        details += "[Module] ";
                        details += module_errors;
                    }
                    if (!ml_errors.empty()) {
                        details += "[ModLoader] ";
                        details += ml_errors;
                    }
                    for (auto &g: groups)
                        if (g.count > 0) {
                            details += "[" + g.ml_id + "] ";
                            details += g.errors;
                        }
                    std::snprintf(error_msg, error_msg_size, "%s", details.c_str());
                    return false;
                }
            }
        }

        std::snprintf(error_msg, error_msg_size, "OK");
        return true;
    }

    // -----------------------------------------------------------------------
    // 包头
    // -----------------------------------------------------------------------

    patchlib::handle net_engine::create_tefkernel_pack() {
        std::vector<uint8_t> payload;
        if (!get_all_versions(payload)) return {};

        const int release = main_state::instance().cur_release();
        const auto total = static_cast<uint32_t>(2 + 1 + 1 + k_magic.size() + 4 + 8 + payload.size());

        std::vector<uint8_t> pkt;
        pkt.reserve(total);
        put_u16(pkt, static_cast<uint16_t>(total));
        pkt.push_back(0x01);
        pkt.push_back(static_cast<uint8_t>(k_magic.size()));
        pkt.insert(pkt.end(), k_magic.begin(), k_magic.end());
        for (int i = 0; i < 4; ++i) pkt.push_back(static_cast<uint8_t>((release >> (i * 8)) & 0xFF));
        for (int i = 0; i < 8; ++i) pkt.push_back(static_cast<uint8_t>((k_version_code >> (i * 8)) & 0xFF));
        pkt.insert(pkt.end(), payload.begin(), payload.end());

        auto arr = patchlib::structs::array<uint8_t>(pkt.size());
        if (!arr) return {};
        arr.copy_from_c(pkt.data(), pkt.size());
        return arr.release();
    }

    patchlib::handle net_engine::create_vanilla_connection_packet(int *out_size) {
        std::array<char, 32> s{};
        const int n = std::snprintf(s.data(), s.size(), "Terraria%d",
                                    main_state::instance().cur_release());
        if (n <= 0 || n > 255) return {};
        std::vector<uint8_t> pkt;
        pkt.push_back(1);
        pkt.push_back(static_cast<uint8_t>(n));
        pkt.insert(pkt.end(), s.data(), s.data() + n);
        auto arr = patchlib::structs::array<uint8_t>(pkt.size());
        if (!arr) return {};
        arr.copy_from_c(pkt.data(), pkt.size());
        if (out_size) *out_size = static_cast<int>(pkt.size());
        return arr.release();
    }

    patchlib::handle net_engine::create_null_connection_packet() {
        static constexpr std::array<uint8_t, 5> k_null = {5, 0, 1, 1, 'a'};
        auto arr = patchlib::structs::array<uint8_t>(k_null.size());
        if (!arr) return {};
        arr.copy_from_c(k_null.data(), k_null.size());
        return arr.release();
    }

    // -----------------------------------------------------------------------
    // 解析 / 发包
    // -----------------------------------------------------------------------

    void net_engine::set_mod_message_handler(const terraria_net_mod_message_cb cb) noexcept {
        mod_cb_ = cb;
    }

    void net_engine::send_tile_action(const int x, const int y, const int action) const {
        resolve_sync_methods();
        if (!m_send_data11_ || x < 0 || y < 0) return;
        constexpr int msg = 17, remote = -1, ignore = -1;
        const int number = action;
        const patchlib::handle text;
        const float n2 = static_cast<float>(x), n3 = static_cast<float>(y), n4 = 0.0f;
        constexpr int n5 = 0, n6 = 0, n7 = 0;
        m_send_data11_.invoke<void>(msg, remote, ignore, text, number,
                                    n2, n3, n4, n5, n6, n7);
    }

    void net_engine::send_tile_square(const int x, const int y, const int width, const int height) const {
        resolve_sync_methods();
        if (!m_send_data11_ || width <= 0 || height <= 0) return;
        const int msg = 20, remote = -1, ignore = -1, number = x;
        const patchlib::handle text;
        const auto n2 = static_cast<float>(y);
        const auto n4 = static_cast<float>(height);
        const auto n3 = static_cast<float>(width);
        constexpr int n5 = 0, n6 = 0, n7 = 0;
        m_send_data11_.invoke<void>(msg, remote, ignore, text, number,
                                    n2, n3, n4, n5, n6, n7);
    }

    void net_engine::send_mod_message(const char *mod_id, const char *channel,
                                      const void *data, int len) const {
        if (!mod_id || !*mod_id || !channel || !*channel) return;
        if (net_mode_now() != 1) {
            log::warn("net: mod message only allowed on client (netMode={})", net_mode_now());
            return;
        }
        if (len < 0) len = 0;
        if (len > 8192) len = 8192;

        constexpr size_t mlen = k_magic.size();
        const size_t idlen = std::strlen(mod_id);
        const size_t chlen = std::strlen(channel);
        const size_t total = 2 + 1 + 1 + mlen + 2 + idlen + 2 + chlen + 4 + static_cast<size_t>(len);

        std::vector<uint8_t> b;
        b.reserve(total);
        put_u16(b, static_cast<uint16_t>(total));
        b.push_back(k_mod_message_id);
        b.push_back(mlen);
        b.insert(b.end(), k_magic.begin(), k_magic.end());
        put_u16(b, static_cast<uint16_t>(idlen));
        b.insert(b.end(), mod_id, mod_id + idlen);
        put_u16(b, static_cast<uint16_t>(chlen));
        b.insert(b.end(), channel, channel + chlen);
        put_u32(b, static_cast<uint32_t>(len));
        if (len > 0)
            b.insert(b.end(), static_cast<const uint8_t *>(data),
                     static_cast<const uint8_t *>(data) + len);

        log::info("net: send mod message {}/{} len={} total={}",
                  mod_id, channel, len, total);
        send_raw_packet(b.data(), static_cast<int>(b.size()));
    }

    connection_type net_engine::parse_tefconnection_packet(const uint8_t *d, const int len,
                                                           const int client) {
        auto fail = [&](const net_error e, const std::string_view msg) {
            client_errors[client] = e;
            std::snprintf(client_error_details[client],
                          sizeof(client_error_details[client]), "%.*s",
                          static_cast<int>(msg.size()), msg.data());
            return connection_type::version_mismatch;
        };
        const auto view = std::span(d, static_cast<size_t>(len));
        size_t off = 1;
        if (off + 1 >= view.size()) return fail(net_error::version, "Invalid packet: too short");
        const uint8_t n = view[off++];
        if (off + n + 12 > view.size()) return fail(net_error::version, "Invalid packet: incomplete");
        if (n != k_magic.size() || std::memcmp(&view[off], k_magic.data(), n) != 0)
            return fail(net_error::version, "Invalid magic string");
        off += n + 4;
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(view[off++]) << (i * 8);
        if (v != k_version_code) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Version mismatch: local=%llu, remote=%llu",
                          static_cast<unsigned long long>(k_version_code), static_cast<unsigned long long>(v));
            return fail(net_error::version, buf);
        }
        const auto cs = static_cast<uint32_t>(view.size() - off);
        if (cs < 4) return fail(net_error::version, "Invalid compressed data");

        char err[512] = {};
        if (!compare_versions(view.data() + off, cs, err, sizeof(err))) {
            client_errors[client] = net_error::mod_required;
            std::snprintf(client_error_details[client],
                          sizeof(client_error_details[client]), "%s", err);
            return connection_type::bad_hash;
        }
        std::snprintf(client_error_details[client],
                      sizeof(client_error_details[client]),
                      "All modules/ModLoaders/Mods match");
        return connection_type::tefkernel;
    }

    bool net_engine::is_vanilla_connection_packet(const uint8_t *d, const int len) {
        if (len < 2) return false;
        const auto view = std::span(d, static_cast<size_t>(len));
        size_t off = 1;
        const uint8_t n = view[off++];
        if (off + n > view.size()) return false;
        const auto s = std::string_view(reinterpret_cast<const char *>(&view[off]), n);
        return s.starts_with("Terraria"); // C++20
    }

    void net_engine::send_raw_packet(const uint8_t *data, const int len) const {
        if (!data || len <= 0) return;
        if (!m_send_raw_) {
#if defined(__ANDROID__)
            if (const auto t = patchlib::type("Telepathy", "Client"))
                m_send_raw_ = t.get_method("Send", 2);
#else
            if (const auto t = patchlib::type("Terraria", "NetMessage"))
                m_send_raw_ = t.get_method("SendPacket", 2);
#endif
        }
        if (!m_send_raw_) {
            log::warn("net: raw send method not resolved");
            return;
        }

        auto arr = patchlib::structs::array<uint8_t>(static_cast<size_t>(len));
        if (!arr) return;
        arr.copy_from_c(data, len);

#if defined(__ANDROID__)
        if (!g_telepathy_client_) {
            static patchlib::field f_conn, f_client;
            if (!f_conn) if (const auto t = patchlib::type("Terraria", "Netplay")) f_conn = t.get_field("Connection");
            if (!f_client)
                if (const auto t = patchlib::type("Terraria", "RemoteConnection"))
                    f_client = t.get_field("Client");
            if (f_conn && f_client) {
                if (const auto conn = f_conn.get_value<patchlib::handle>())
                    g_telepathy_client_ = f_client.get_value<patchlib::handle>(conn);
            }
        }
        if (!g_telepathy_client_) {
            log::warn("net: Telepathy.Client not ready, packet dropped");
            return;
        }
        m_send_raw_[g_telepathy_client_](arr, len);
#else
        constexpr int remote = 256;
        m_send_raw_.invoke<void>(arr, remote);
#endif
    }

    int net_engine::net_mode_now() const noexcept {
        return f_net_mode_ ? f_net_mode_.get_value<int>() : 0;
    }

    void net_engine::resolve_sync_methods() const {
        if (m_send_data11_ && m_send_tile_square_) return;
        const auto t = patchlib::type("Terraria", "NetMessage");
        if (!t) return;
        if (!m_send_data11_) m_send_data11_ = t.get_method("SendData", 11);
        if (!m_send_tile_square_) m_send_tile_square_ = t.get_method("SendTileSquare", 6);
    }

    // -----------------------------------------------------------------------
    // Mod 消息分发
    // -----------------------------------------------------------------------

    bool net_engine::handle_mod_message(const uint8_t *data, const int length, const int start,
                                        patch_handle_t instance) const {
        if (length < 1) return false;
        const auto view = std::span(data + start, static_cast<size_t>(length));
        size_t off = 1; // 跳过消息号
        auto remain = [&] { return view.size() - off; };

        if (remain() < 1) return false;
        const uint8_t magic_len = view[off++];
        if (magic_len != k_magic.size()) return false;
        if (remain() < magic_len + 2 + 2 + 4) return false;
        if (std::memcmp(&view[off], k_magic.data(), magic_len) != 0) return false;
        off += magic_len;

        if (remain() < 2) return true;
        const int mod_len = view[off] | (view[off + 1] << 8);
        off += 2;
        if (mod_len <= 0 || static_cast<size_t>(mod_len) > remain() || mod_len > 127) return true;
        std::string mod_id(reinterpret_cast<const char *>(&view[off]), mod_len);
        off += static_cast<size_t>(mod_len);

        if (remain() < 2) return true;
        const int ch_len = view[off] | (view[off + 1] << 8);
        off += 2;
        if (ch_len <= 0 || static_cast<size_t>(ch_len) > remain() || ch_len > 127) return true;
        std::string channel(reinterpret_cast<const char *>(&view[off]), ch_len);
        off += static_cast<size_t>(ch_len);

        if (remain() < 4) return true;
        uint32_t data_len = static_cast<uint32_t>(view[off]) | (static_cast<uint32_t>(view[off + 1]) << 8) |
                            (static_cast<uint32_t>(view[off + 2]) << 16) | (static_cast<uint32_t>(view[off + 3]) << 24);
        off += 4;
        if (data_len > remain()) data_len = static_cast<uint32_t>(remain());

        int client = -1;
        if (f_who_am_i_) client = f_who_am_i_.get_value<int>(instance);

        log::info("net: mod message mod={} channel={} len={} client={}",
                  mod_id, channel, data_len, client);
        if (mod_cb_)
            mod_cb_(mod_id.c_str(), channel.c_str(),
                    &view[off], static_cast<int>(data_len), client);
        return true;
    }

    // -----------------------------------------------------------------------
    // 钩子：收包
    // -----------------------------------------------------------------------

    bool net_engine::get_data_hook(patch_handle_t this_handle, void **args,
                                   const patch_method_signature_t *, void *) {
        auto &self = instance();
        uint8_t msg_type = 0;
        std::vector<uint8_t> local;
        const uint8_t *view = nullptr;
        int start = 0, length = 0;
        patchlib::handle read_buf;
        int buffer_max = -1;

#if defined(__ANDROID__)
        struct il2_cpp_array_t {
            void *m_class, *m_monitor, *m_bounds;
            uint32_t m_length;
        };
        view = static_cast<const uint8_t *>(*static_cast<patch_handle_t *>(args[0])) + sizeof(il2_cpp_array_t);
        length = *static_cast<int *>(args[1]);
        if (length < 1) return false;
        msg_type = view[0];
#else
        start = *static_cast<int *>(args[0]);
        length = *static_cast<int *>(args[1]);
        if (self.f_read_buffer_)
            read_buf = self.f_read_buffer_.get_value<patchlib::handle>(this_handle);
        if (self.f_read_buffer_max_)
            buffer_max = self.f_read_buffer_max_.get_value<int>();
        if (read_buf) {
            if (buffer_max <= 0) buffer_max = length + 8;
            local.resize(static_cast<size_t>(buffer_max));
            patchlib::structs::array<uint8_t>(read_buf, false)
                    .copy_to_c(local.data(), static_cast<size_t>(buffer_max));
            view = local.data();
            if (start >= 0 && start < buffer_max) msg_type = view[start];
        }
#endif

        static uint8_t seen[256] = {};
        if (!seen[msg_type]) {
            seen[msg_type] = 1;
            log::info("net: first packet msg={} len={}", msg_type, length);
        }

        if (msg_type == k_mod_message_id) {
            if (view && self.handle_mod_message(view, length, start, this_handle))
                return true;
        }

        if (msg_type == 1) {
            int client = -1;
            if (self.f_who_am_i_) client = self.f_who_am_i_.get_value<int>(this_handle);
            if (client < 0 || client >= 256) return true;

            self.client_connections[client] = connection_type::none;
            self.client_errors[client] = net_error::none;
            self.client_error_details[client][0] = '\0';

            self.client_connections[client] =
                    self.parse_tefconnection_packet(view + start, length, client);
            log::info("net: msg=1 client={} start={} length={} type={}",
                      client, start, length, static_cast<int>(self.client_connections[client]));

            if (self.client_connections[client] != connection_type::none) {
                if (self.client_errors[client] != net_error::none) {
                    log::warn("net: reject client {}: {}", client,
                              self.client_error_details[client]);
                    return false;
                }
                int vanilla_size = 0;
                auto vanilla = create_vanilla_connection_packet(&vanilla_size);
                if (!vanilla) return false;
#if defined(__ANDROID__)
                *static_cast<patch_handle_t *>(args[0]) = vanilla.release();
#else
                auto rb = patchlib::structs::array<uint8_t>(read_buf, false);
                const auto va = patchlib::structs::array<uint8_t>(vanilla, false);
                for (int i = 0; i < vanilla_size; ++i) {
                    rb.set(start + i, va.at(static_cast<size_t>(i)));
                }
#endif
                return false;
            }

            if (is_vanilla_connection_packet(view + start, length)) {
                self.client_connections[client] = connection_type::vanilla;
                self.client_errors[client] = net_error::mod_required;
                std::snprintf(self.client_error_details[client],
                              sizeof(self.client_error_details[client]),
                              "Vanilla client detected - TEFKernel required");
                auto null_arr = create_null_connection_packet();
#if defined(__ANDROID__)
                *static_cast<patch_handle_t *>(args[0]) = null_arr.release();
                *static_cast<int *>(args[1]) = 5;
#else
                *static_cast<int *>(args[1]) = 5;
                auto rb = patchlib::structs::array<uint8_t>(read_buf, false);
                const auto na = patchlib::structs::array<uint8_t>(null_arr, false);
                for (int i = 0; i < 5; ++i)
                    rb.set(start + i, na.at(static_cast<size_t>(i)));
#endif
                return false;
            }
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // 钩子：发包
    // -----------------------------------------------------------------------

    bool net_engine::send_data_hook(patch_handle_t, void **args,
                                    const patch_method_signature_t *, void *) {
        if (!args || !args[3]) return false;
        const int msg_type = *static_cast<int *>(args[0]);
        const int remote = *static_cast<int *>(args[1]);
        if (msg_type != 2 || remote < 0 || remote >= 256) return false;

        auto &self = instance();
        if (self.client_errors[remote] != net_error::mod_required &&
            self.client_errors[remote] != net_error::version)
            return false;

        char error_msg[512];
        const char *type = self.client_errors[remote] == net_error::version
                               ? "Version mismatch"
                               : "Mod mismatch";
        std::snprintf(error_msg, sizeof(error_msg), "[TEFKernel] %s: %s",
                      type, self.client_error_details[remote]);

        const patchlib::structs::string s(error_msg);
        if (!s) return false;
        if (auto text = self.m_network_text_from_literal_.invoke<patchlib::handle>(s))
            *static_cast<patch_handle_t *>(args[3]) = text.release();
        return false;
    }

    // -----------------------------------------------------------------------
    // 钩子：连接包替换
    // -----------------------------------------------------------------------

    bool net_engine::send_hook(patch_handle_t this_handle, void **args,
                               const patch_method_signature_t *, void *) {
        auto &self = instance();
#if defined(__ANDROID__)
        if (!self.g_telepathy_client_ && this_handle)
            self.g_telepathy_client_ = patchlib::handle(this_handle, false);
#endif
        if (!args || !args[0]) return false;
        const auto arr = patchlib::structs::array<uint8_t>(*static_cast<patch_handle_t *>(args[0]), false);
        if (!arr || arr.length() < 3) return false;
        if (arr.at(2) != 1) return false;

        auto pack = create_tefkernel_pack();
        if (!pack) return false;
        *static_cast<patch_handle_t *>(args[0]) = pack.release();
#if defined(__ANDROID__)
        *static_cast<int *>(args[1]) = static_cast<int>(patchlib_array_length(*static_cast<patch_handle_t *>(args[0])));
#endif
        return false;
    }

    // -----------------------------------------------------------------------
    // 生命周期
    // -----------------------------------------------------------------------

    void net_engine::early_init() {
        if (module::module_registry::instance().count() == 0 &&
            ml::mod_loader_registry::instance().count() == 0) {
            log::info("net: no modules/ModLoaders, version isolation disabled");
            return;
        }
        for (auto &d: client_error_details) d[0] = '\0';

        const auto mb = patchlib::type("Terraria", "MessageBuffer");
        const auto nm = patchlib::type("Terraria", "NetMessage");
        const auto nt = patchlib::type("Terraria.Localization", "NetworkText");
        if (!mb || !nm || !nt) {
            log::error("net: required types missing");
            return;
        }

        m_network_text_from_literal_ = nt.get_method("FromLiteral", 1);
        f_who_am_i_ = mb.get_field("whoAmI");
        if (const auto main = patchlib::type("Terraria", "Main"))
            f_net_mode_ = main.get_field("netMode");

#if defined(__ANDROID__)
        m_process_data_ = mb.get_method("ProcessData", 3);
        m_client_send_ = patchlib::type("Telepathy", "Client").get_method("Send", 2);

        if (patchlib_install_typed_hook(m_process_data_.raw(),
                                        reinterpret_cast<void *>(&process_data_detour),
                                        reinterpret_cast<void **>(&g_orig_process_data)))
            log::info("net: ProcessData typed hook installed");
        else
            log::error("net: ProcessData typed hook failed");

        if (const auto m = nm.get_method("SendData", 11))
            if (patchlib_install_typed_hook(m.raw(),
                                            reinterpret_cast<void *>(&send_data_detour),
                                            reinterpret_cast<void **>(&g_orig_send_data)))
                log::info("net: SendData typed hook installed");
            else
                log::error("net: SendData typed hook failed");

        if (patchlib_install_typed_hook(m_client_send_.raw(),
                                        reinterpret_cast<void *>(client_send_detour),
                                        reinterpret_cast<void **>(g_orig_client_send)))
            log::info("net: Telepathy.Client.Send typed hook installed");
        else
            log::error("net: Telepathy.Client.Send typed hook failed");
#else
        f_read_buffer_ = mb.get_field("readBuffer");
        f_read_buffer_max_ = mb.get_field("readBufferMax");

        if (const auto m = mb.get_method("GetData", 0))
            h_get_data_ = patchlib_install_prepost_hook(
                m.raw(), get_data_hook, nullptr);
        if (const auto m = nm.get_method("SendData", 11))
            h_send_data_ = patchlib_install_prepost_hook(
                m.raw(), send_data_hook, nullptr);
        if (const auto m = nm.get_method("SendPacket", 2))
            h_send_ = patchlib_install_prepost_hook(
                m.raw(), send_hook, nullptr);
#endif
    }

    void net_engine::shutdown() noexcept {
        mod_cb_ = nullptr;
    }

    bool net_engine::dispatch_process_data(void *self, void **args) {
        return get_data_hook(self, args, nullptr, nullptr);
    }

    void net_engine::dispatch_send_data(void **args) {
        send_data_hook(nullptr, args, nullptr, nullptr);
    }

    void net_engine::dispatch_client_send(void **args) {
        send_hook(nullptr, args, nullptr, nullptr);
    }
}
