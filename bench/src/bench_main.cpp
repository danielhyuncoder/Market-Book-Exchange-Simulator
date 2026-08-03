#include <benchmark/benchmark.h>
#include "order_book.hpp"
#include <boost/asio.hpp>

using namespace ORDER_BOOK_PACKAGE;

static ORDER make_order(LL id, ORDER_TYPE side, LL price, LL qty) {
    ORDER o;
    o.order_id = id;
    o.order_type = side;
    o.price_level = price;
    o.qty = qty;
    o.request_type = REQUEST_TYPE::SEND_ORDER;
    return o;
}

static void BM_SendOrder_NoMatch(benchmark::State& state) {
    ORDER_BOOK book;
    LL id = 1;
    for (auto _ : state) {
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, 100, 10));
    }
}
BENCHMARK(BM_SendOrder_NoMatch);

static void BM_SendOrder_FullMatch(benchmark::State& state) {
    ORDER_BOOK book;
    LL id = 1;
    for (auto _ : state) {
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::SELL, 100, 10));
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, 100, 10));
        book.UPDATE_BOOK();
    }
}
BENCHMARK(BM_SendOrder_FullMatch);

static void BM_CreateSnapshot(benchmark::State& state) {
    ORDER_BOOK book;
    for (LL i = 0; i < SNAPSHOT_LEN; i++) {
        book.SEND_ORDER(make_order(i, ORDER_TYPE::BUY, 100 - i, 10));
        book.SEND_ORDER(make_order(i + 1000, ORDER_TYPE::SELL, 200 + i, 10));
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.CREATE_SNAPSHOT());
    }
}
BENCHMARK(BM_CreateSnapshot);

static void BM_SendOrder_AtDepth(benchmark::State& state) {
    ORDER_BOOK book;
    LL depth = state.range(0);
    LL id = 1;
    for (LL i = 0; i < depth; i++) {
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, i, 10));
    }
    for (auto _ : state) {
        LL price = id % depth;
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, price, 10));
    }
}
BENCHMARK(BM_SendOrder_AtDepth)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000);

// marketbook benchmarking

static std::string make_symbol(int i) {
    char buf[5];
    snprintf(buf, sizeof(buf), "S%03d", i % 1000);   // 4 chars, matches SYMBOL_BYTES
    return std::string(buf);
}

static void BM_Shard_SameSymbol(benchmark::State& state) {
    static boost::asio::io_context ctx;
    static std::unique_ptr<MARKET_BOOK> mb;
    if (state.thread_index() == 0) {
        mb = std::make_unique<MARKET_BOOK>(ctx, std::vector<std::string>{"APPL"});
    }
    LL id = state.thread_index() * 10000000LL;
    for (auto _ : state) {
        ORDER o = make_order(id++, ORDER_TYPE::BUY, 100, 10);
        std::memcpy(o.symbol, "APPL", SYMBOL_BYTES);   // every thread hammers the SAME shard
        mb->process_one(o);
    }
}
BENCHMARK(BM_Shard_SameSymbol)->ThreadRange(1, 16);

static void BM_Shard_Spread(benchmark::State& state) {
    static boost::asio::io_context ctx;
    static std::unique_ptr<MARKET_BOOK> mb;
    if (state.thread_index() == 0) {
        std::vector<std::string> symbols;
        for (int i = 0; i < 16; i++) symbols.push_back(make_symbol(i));
        mb = std::make_unique<MARKET_BOOK>(ctx, symbols);
    }
    LL id = state.thread_index() * 10000000LL;
    std::string sym = make_symbol(state.thread_index());   // each thread owns a distinct symbol -> distinct shard
    for (auto _ : state) {
        ORDER o = make_order(id++, ORDER_TYPE::BUY, 100, 10);
        std::memcpy(o.symbol, sym.data(), SYMBOL_BYTES);
        mb->process_one(o);
    }
}
BENCHMARK(BM_Shard_Spread)->ThreadRange(1, 16);