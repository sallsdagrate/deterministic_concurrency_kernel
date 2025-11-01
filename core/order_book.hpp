#include <map>
#include <unordered_map>
#include <deque>
#include <cstdint>
#include <vector>
#include <chrono>
#include <iostream>
#include <iterator>

#include "order_book_types.hpp"

using BookMap = std::map<Price, std::deque<OrderId>>;
using SellIt = BookMap::iterator;
using BuyRIt = BookMap::reverse_iterator;

class OrderBook {
    public:

    explicit OrderBook() {
        if (!m_sell_book.empty()) { 
            m_best_sell_it = m_sell_book.begin();
            m_has_best_sell = true;
        }
        if (!m_buy_book.empty())  {
            m_best_buy_it = m_buy_book.rbegin();
            m_has_best_buy = true;
        }
    }

    bool on_new(const Event& event, std::vector<Trade>& trades_out) {

        if (!(event.price > 0 && event.quantity > 0)) return false;

        if (event.side == Side::Buy) {
            // look for best sell price (lowest sell)
            Price best_sell_price;
            
            int32_t quantity_remaining {event.quantity};
            while (fix_best_sell(best_sell_price) && quantity_remaining > 0) {
                // check order sells <= buying price
                if (best_sell_price > event.price) break;
                
                auto& price_queue = m_sell_book.at(best_sell_price);
                
                // continue buying from this order
                OrderId oid {price_queue.front()};
                Order& maker = m_order_index.at(oid);
                
                int32_t qty_bought {std::min(quantity_remaining, maker.quantity_remaining)};
                maker.quantity_remaining -= qty_bought;
                quantity_remaining -= qty_bought;

                // add trade to output
                trades_out.emplace_back(
                    maker.order_id, event.order_id, best_sell_price, qty_bought, std::chrono::steady_clock::now()
                );

                // if order is now empty then delete it
                if (maker.quantity_remaining == 0) {
                    price_queue.pop_front();
                    m_order_index.erase(oid);
                }
                // // if price is now empty then delete it
                // if (price_queue.empty()) m_sell_book.erase(best_sell_price);
            }

            // if still leftover to buy then add to buy book
            if (quantity_remaining > 0) {
                m_order_index.try_emplace(
                    event.order_id,
                    Order{event.order_id, event.side, event.price, quantity_remaining, event.seq, true}
                );
                m_buy_book[event.price].push_back(event.order_id);
                
                // update cache
                if (!m_has_best_buy || event.price > m_best_buy_it->first) {
                    auto it = m_buy_book.find(event.price);
                    m_best_buy_it = make_reverse_iterator(std::next(it));
                    m_has_best_buy = true;
                }
            }


            return true;
        }
        else if (event.side == Side::Sell) {
            // look for best sell price (lowest sell)
            Price best_buy_price;
            
            int32_t quantity_remaining {event.quantity};
            while (fix_best_buy(best_buy_price) && quantity_remaining > 0) {
                // check order buys >= selling price
                if (best_buy_price < event.price) break;
                
                // continue buying
                auto& price_queue = m_buy_book.at(best_buy_price);
                OrderId oid {price_queue.front()};
                Order& maker = m_order_index.at(oid);

                int32_t qty_bought {std::min(quantity_remaining, maker.quantity_remaining)};
                maker.quantity_remaining -= qty_bought;
                quantity_remaining -= qty_bought;

                // add trade to output
                trades_out.emplace_back(
                    event.order_id, maker.order_id, best_buy_price, qty_bought, std::chrono::steady_clock::now()
                );

                // if order is now empty then delete it
                if (maker.quantity_remaining == 0) {
                    price_queue.pop_front();
                    m_order_index.erase(oid);
                }
                // // if price is now empty then delete it
                // if (price_queue.empty()) m_buy_book.erase(best_buy_price);
            }

            // if still leftover to buy then add to buy book
            if (quantity_remaining > 0) {
                m_order_index.try_emplace(
                    event.order_id,
                    Order{event.order_id, event.side, event.price, quantity_remaining, event.seq, true}
                );
                m_sell_book[event.price].push_back(event.order_id);
                
                // update cache
                if (!m_has_best_sell || event.price < m_best_sell_it->first) {
                    m_best_sell_it = m_sell_book.find(event.price);
                    m_has_best_sell = true;
                }
            }


            return true;
        }
        return false;
    }

    OrderBook& operator -= (const OrderId id) {
        if (!on_cancel(id)) {};
        return *this;
    }
    
    bool on_cancel(const OrderId id) { 
        auto it = m_order_index.find(id);
        if (it == m_order_index.end()) return false;
        it->second.active = false;
        return true;
    }
    
    bool on_replace(const Event& event, std::vector<Trade>& trades_out) { 
        if (!on_cancel(event.order_id)) return false;
        return on_new(event, trades_out);
    }

    
    // bool find_best_price(Price& return_price, std::iterator)
    void log_books() const{
        std::cout << "\nBooks\n----\nSell\nPrice | Quantity(Order Id)\n";
        for(auto it = m_sell_book.rbegin(); it != m_sell_book.rend(); it++) {
            std::cout << it->first << " | ";
            for (OrderId oid : it->second) {
                std::cout << m_order_index.at(oid).quantity_remaining << "(" << oid << (m_order_index.at(oid).active ? "" : "/cancelled") << "), ";
            }
            std::cout << "\n";
        }
        
        std::cout << "Buy\nPrice | Quantity(Order Id)\n";
        for(auto it = m_buy_book.rbegin(); it != m_buy_book.rend(); it++) {
            std::cout << it->first << " | ";
            for (OrderId oid : it->second) {
                std::cout << m_order_index.at(oid).quantity_remaining << "(" << oid << (m_order_index.at(oid).active ? "" : "/cancelled") << "), ";
            }
            std::cout << "\n";
        }

        std::cout << "\n";
    }
    
    private:
    
    BookMap m_sell_book;
    BookMap m_buy_book;
    std::unordered_map<OrderId, Order> m_order_index;
    
    SellIt m_best_sell_it;
    BuyRIt m_best_buy_it;
    bool m_has_best_sell {false};
    bool m_has_best_buy {false};
    
    void clean_book (BookMap& book, const std::vector<Price>& prices_to_delete) {
        for (Price p : prices_to_delete) book.erase(p);
    }
    
    // Cache aware best price functions
    bool fix_best_sell(Price& out) {
        while (m_has_best_sell) {
            auto& q = m_best_sell_it->second;
            
            while (!q.empty()) {
                OrderId oid = q.front();
                auto it = m_order_index.find(oid);
                if (it == m_order_index.end() || !it->second.active) {
                    if (it != m_order_index.end()) m_order_index.erase(it);
                    q.pop_front();
                }
                else {
                    out = m_best_sell_it->first;
                    return true;
                }
            }
            
            auto next = std::next(m_best_sell_it);
            m_sell_book.erase(m_best_sell_it);
            if (next == m_sell_book.end()) {
                m_has_best_sell = false;
                return false;
            }
            m_best_sell_it = next;
        }
        return false;
    }
    
    bool fix_best_buy(Price& out) {
        while (m_has_best_buy) {
            auto& q = m_best_buy_it->second;
            
            while (!q.empty()) {
                OrderId oid = q.front();
                auto it = m_order_index.find(oid);
                if (it == m_order_index.end() || !it->second.active) {
                    if (it != m_order_index.end()) m_order_index.erase(it);
                    q.pop_front();
                }
                else {
                    out = m_best_buy_it->first;
                    return true;
                }
            }
            
            auto it = std::prev(m_best_buy_it.base());
            it = m_buy_book.erase(it);
            if (m_buy_book.empty()) {
                m_has_best_buy = false;
                return false;
            }
            m_best_buy_it = (it == m_buy_book.end()) ? m_buy_book.rbegin() : BuyRIt(it);
        }
        return false;
    }

    // Non-cache aware best price functions, scans every time
    bool find_best_sell_price(Price& return_price) {
        std::vector<Price> toDelete;
        bool ret {false};
    
        for (auto price = m_sell_book.begin(); price != m_sell_book.end(); price++) {
            // iterate from lowest to highest
            // remove inactive orders
            auto& q = (price->second);
            while (!q.empty() && !m_order_index.at(q.front()).active) {
                m_order_index.erase(q.front());
                q.pop_front();
            };
            // return best sell price
            if (!q.empty()) {
                return_price = price->first;
                ret = true;
                break;
            }
            else {
                toDelete.emplace_back(price->first);
            }
        }
    
        clean_book(m_sell_book, toDelete);
        return ret;
    }
    
    bool find_best_buy_price(Price& return_price) {
        std::vector<Price> toDelete;
        bool ret {false};
    
        for (auto price = m_buy_book.rbegin(); price != m_buy_book.rend(); price++) {
            // iterate from highest to lowest
            // remove inactive orders
            auto& q = (price->second);
            while (!q.empty() && !m_order_index.at(q.front()).active) {
                m_order_index.erase(q.front());
                q.pop_front();
            };
            // return best sell price
            if (!q.empty()) {
                return_price = price->first;
                ret = true;
                break;
            }
            else {
                toDelete.emplace_back(price->first);
            }
        }
    
        clean_book(m_buy_book, toDelete);
        return ret;
    }
};

