// #include <arpa/inet.h>
// #include <benchmark/benchmark.h>
//
// #include <cstddef>
// #include <span>
//
// #include "../../utils.hpp"
// #include "tx/net/taifex/parser.hpp"
// #include "tx/net/taifex/wire_format.hpp"
//
// namespace tx::net::taifex::bench {
//
// //
// ============================================================================
// // Parket Header 解析
// //
// ============================================================================
// std::vector<std::byte> prepare_packet_header() {
//   size_t total_size = sizeof(PacketHeader) + 2 * sizeof(R02TradeWire);
//   std::vector<std::byte> buffer(total_size, std::byte{0});
//
//   auto* hdr = reinterpret_cast<PacketHeader*>(buffer.data());
//   hdr->esc_code = 0x1B;
//   hdr->packet_version = 0x01;
//   hdr->packet_length = htons(static_cast<uint16_t>(total_size));
//   hdr->msg_count = htons(2);
//   hdr->pkt_seq_num = htonl(12345);
//   hdr->channel_id = htons(1);
//   hdr->send_time = htonl(13305500);
//
//   return buffer;
// }
//
// static void BM_Parse_PacketHeader(benchmark::State& state) {
//   auto data = prepare_packet_header();
//
//   // 預先驗證一次，確保測試數據沒問題
//   if (!parse_packet_header(data)) {
//     state.SkipWithError("Parse failed");
//     return;
//   }
//
//   for (auto _ : state) {
//     auto result = parse_packet_header(data);
//     benchmark::DoNotOptimize(result);
//     benchmark::ClobberMemory();
//   }
//
//   state.SetItemsProcessed(state.iterations());
// }
// BENCHMARK(BM_Parse_PacketHeader);
//
// //
// ============================================================================
// // R02 Trade 解析
// //
// ============================================================================
// std::vector<std::byte> prepare_r02_data() {
//   std::vector<std::byte> buffer(sizeof(R02TradeWire), std::byte{0});
//   auto* wire = reinterpret_cast<R02TradeWire*>(buffer.data());
//   wire->header.msg_kind = 'R';
//   wire->header.msg_type = '2';
//   wire->header.msg_length = htons(sizeof(R02TradeWire));
//
//   wire->match_price = htonl(21000);
//   wire->match_qty = htonl(50);
//   wire->total_volume = htonl(123456);
//   wire->match_time = htobe64(13305512345678ULL);  // HHMMSSuuuuuu
//   wire->side = 1;                                 // 買方主動
//
//   return buffer;
// }
//
// static void BM_Parse_R02_Trade(benchmark::State& state) {
//   auto data = prepare_r02_data();
//
//   if (!parse_r02_trade(data)) {
//     state.SkipWithError("Parse failed");
//     return;
//   }
//
//   for (auto _ : state) {
//     auto result = parse_r02_trade(std::span(data));
//     benchmark::DoNotOptimize(result);
//     benchmark::ClobberMemory();
//   }
//
//   state.SetItemsProcessed(state.iterations());
// }
// BENCHMARK(BM_Parse_R02_Trade);
//
// //
// ============================================================================
// // R06 Snapshot 解析
// //
// ============================================================================
// std::vector<std::byte> prepare_r06_data() {
//   std::vector<std::byte> buffer(sizeof(R06SnapshotWire), std::byte{0});
//   auto* wire = reinterpret_cast<R06SnapshotWire*>(buffer.data());
//
//   wire->header.msg_kind = 'R';
//   wire->header.msg_type = '6';
//   wire->header.msg_length = htons(sizeof(R06SnapshotWire));
//
//   for (int i = 0; i < 5; ++i) {
//     wire->bid_entries[i].price = htonl(21000 - i * 10);
//     wire->bid_entries[i].quantity = htonl(100 + i * 10);
//     wire->bid_entries[i].order_count = htonl(5 + i);
//
//     wire->ask_entries[i].price = htonl(21050 + i * 10);
//     wire->ask_entries[i].quantity = htonl(80 + i * 10);
//     wire->ask_entries[i].order_count = htonl(4 + i);
//   }
//
//   return buffer;
// }
//
// static void BM_Parse_R06_Snapshot(benchmark::State& state) {
//   auto data = prepare_r06_data();
//
//   if (!parse_r06_snapshot(std::span(data))) {
//     state.SkipWithError("Input data is invalid for parser");
//     return;
//   }
//
//   for (auto _ : state) {
//     auto result = parse_r06_snapshot(std::span(data));
//
//     benchmark::DoNotOptimize(result);
//     benchmark::ClobberMemory();  // 防止快取
//   }
//
//   state.SetItemsProcessed(state.iterations());
// }
// BENCHMARK(BM_Parse_R06_Snapshot);
//
// }  // namespace tx::net::taifex::bench
