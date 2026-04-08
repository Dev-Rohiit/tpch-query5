#include "query5.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>

// Helper for string tokenization
void splitCSV(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    size_t start = 0;
    size_t end = line.find('|');
    while (end != std::string::npos) {
        tokens.push_back(line.substr(start, end - start));
        start = end + 1;
        end = line.find('|', start);
    }
}

// Function to parse command line arguments
bool parseArgs(int argc, char* argv[], std::string& r_name, std::string& start_date, std::string& end_date, int& num_threads, std::string& table_path, std::string& result_path) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--r_name" && i + 1 < argc) r_name = argv[++i];
        else if (arg == "--start_date" && i + 1 < argc) start_date = argv[++i];
        else if (arg == "--end_date" && i + 1 < argc) end_date = argv[++i];
        else if (arg == "--threads" && i + 1 < argc) num_threads = std::stoi(argv[++i]);
        else if (arg == "--table_path" && i + 1 < argc) table_path = argv[++i];
        else if (arg == "--result_path" && i + 1 < argc) result_path = argv[++i];
    }
    return !r_name.empty() && !start_date.empty() && !end_date.empty() && num_threads > 0 && !table_path.empty() && !result_path.empty();
}

bool loadTable(const std::string& path, std::vector<std::map<std::string, std::string>>& data, const std::vector<std::string>& headers, const std::vector<bool>& keep) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    std::vector<std::string> tokens;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        splitCSV(line, tokens);
        std::map<std::string, std::string> row;
        for (size_t i = 0; i < tokens.size() && i < headers.size(); ++i) {
            if (keep[i]) row[headers[i]] = tokens[i];
        }
        data.push_back(std::move(row));
    }
    return true;
}

bool loadOrders(const std::string& path, std::vector<std::map<std::string, std::string>>& data) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    std::vector<std::string> tokens;
    std::vector<std::string> headers = {"o_orderkey", "o_custkey", "o_orderstatus", "o_totalprice", "o_orderdate", "o_orderpriority", "o_clerk", "o_shippriority", "o_comment"};
    std::vector<bool> keep = {true, true, false, false, true, false, false, false, false};
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        splitCSV(line, tokens);
        if (tokens.size() > 4) {
            const std::string& od = tokens[4];
            if (od < "1994-01-01" || od >= "1995-01-01") continue;
        }
        std::map<std::string, std::string> row;
        for (size_t i = 0; i < tokens.size() && i < headers.size(); ++i) {
            if (keep[i]) row[headers[i]] = tokens[i];
        }
        data.push_back(std::move(row));
    }
    return true;
}

bool loadLineitem(const std::string& path, std::vector<std::map<std::string, std::string>>& data, const std::unordered_set<std::string>& valid_orders) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    std::vector<std::string> tokens;
    std::vector<std::string> headers = {"l_orderkey", "l_partkey", "l_suppkey", "l_linenumber", "l_quantity", "l_extendedprice", "l_discount", "l_tax", "l_returnflag", "l_linestatus", "l_shipdate", "l_commitdate", "l_receiptdate", "l_shipinstruct", "l_shipmode", "l_comment"};
    std::vector<bool> keep = {true, false, true, false, false, true, true, false, false, false, false, false, false, false, false, false};
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        splitCSV(line, tokens);
        if (tokens.empty()) continue;
        if (valid_orders.find(tokens[0]) == valid_orders.end()) continue;
        std::map<std::string, std::string> row;
        for (size_t i = 0; i < tokens.size() && i < headers.size(); ++i) {
            if (keep[i]) row[headers[i]] = tokens[i];
        }
        data.push_back(std::move(row));
    }
    return true;
}

// Function to read TPCH data from the specified paths
bool readTPCHData(const std::string& table_path, std::vector<std::map<std::string, std::string>>& customer_data, std::vector<std::map<std::string, std::string>>& orders_data, std::vector<std::map<std::string, std::string>>& lineitem_data, std::vector<std::map<std::string, std::string>>& supplier_data, std::vector<std::map<std::string, std::string>>& nation_data, std::vector<std::map<std::string, std::string>>& region_data) {
    auto rp = [](const std::string& p, const std::string& ext) {
        if (p.empty()) return ext;
        if (p.back() == '/' || p.back() == '\\') return p + ext;
        return p + "/" + ext;
    };
    
    std::vector<std::string> c_h = {"c_custkey", "c_name", "c_address", "c_nationkey", "c_phone", "c_acctbal", "c_mktsegment", "c_comment"};
    std::vector<bool> c_k = {true, false, false, true, false, false, false, false}; // custkey, nationkey
    if (!loadTable(rp(table_path, "customer.tbl"), customer_data, c_h, c_k)) { std::cerr << "Failed to read customer" << std::endl; return false; }

    if (!loadOrders(rp(table_path, "orders.tbl"), orders_data)) { std::cerr << "Failed to read orders" << std::endl; return false; }
    
    std::unordered_set<std::string> valid_orders;
    for (const auto& o : orders_data) {
        auto ok_it = o.find("o_orderkey");
        if (ok_it != o.end()) valid_orders.insert(ok_it->second);
    }

    if (!loadLineitem(rp(table_path, "lineitem.tbl"), lineitem_data, valid_orders)) { std::cerr << "Failed to read lineitem" << std::endl; return false; }

    std::vector<std::string> s_h = {"s_suppkey", "s_name", "s_address", "s_nationkey", "s_phone", "s_acctbal", "s_comment"};
    std::vector<bool> s_k = {true, false, false, true, false, false, false}; // suppkey, nationkey
    if (!loadTable(rp(table_path, "supplier.tbl"), supplier_data, s_h, s_k)) { std::cerr << "Failed to read supplier" << std::endl; return false; }

    std::vector<std::string> n_h = {"n_nationkey", "n_name", "n_regionkey", "n_comment"};
    std::vector<bool> n_k = {true, true, true, false}; // nationkey, name, regionkey
    if (!loadTable(rp(table_path, "nation.tbl"), nation_data, n_h, n_k)) { std::cerr << "Failed to read nation" << std::endl; return false; }

    std::vector<std::string> r_h = {"r_regionkey", "r_name", "r_comment"};
    std::vector<bool> r_k = {true, true, false}; // regionkey, name
    if (!loadTable(rp(table_path, "region.tbl"), region_data, r_h, r_k)) { std::cerr << "Failed to read region" << std::endl; return false; }

    return true;
}

// Function to execute TPCH Query 5 using multithreading
bool executeQuery5(const std::string& r_name, const std::string& start_date, const std::string& end_date, int num_threads, const std::vector<std::map<std::string, std::string>>& customer_data, const std::vector<std::map<std::string, std::string>>& orders_data, const std::vector<std::map<std::string, std::string>>& lineitem_data, const std::vector<std::map<std::string, std::string>>& supplier_data, const std::vector<std::map<std::string, std::string>>& nation_data, const std::vector<std::map<std::string, std::string>>& region_data, std::map<std::string, double>& results) {
    
    std::string region_key = "";
    for (const auto& r : region_data) {
        auto r_name_it = r.find("r_name");
        auto r_key_it = r.find("r_regionkey");
        if (r_name_it != r.end() && r_name_it->second == r_name && r_key_it != r.end()) {
            region_key = r_key_it->second;
            break;
        }
    }
    if (region_key.empty()) return true;

    std::unordered_map<std::string, std::string> valid_nations; 
    for (const auto& n : nation_data) {
        auto n_reg_it = n.find("n_regionkey");
        auto n_nat_it = n.find("n_nationkey");
        auto n_name_it = n.find("n_name");
        if (n_reg_it != n.end() && n_reg_it->second == region_key && n_nat_it != n.end() && n_name_it != n.end()) {
            valid_nations[n_nat_it->second] = n_name_it->second;
        }
    }
    if (valid_nations.empty()) return true;

    std::unordered_map<std::string, std::string> cust_nation;
    for (const auto& c : customer_data) {
        auto iter = c.find("c_nationkey");
        auto cust_it = c.find("c_custkey");
        if (iter != c.end() && cust_it != c.end() && valid_nations.count(iter->second)) {
            cust_nation[cust_it->second] = iter->second;
        }
    }

    std::unordered_map<std::string, std::string> supp_nation;
    for (const auto& s : supplier_data) {
        auto iter = s.find("s_nationkey");
        auto supp_it = s.find("s_suppkey");
        if (iter != s.end() && supp_it != s.end() && valid_nations.count(iter->second)) {
            supp_nation[supp_it->second] = iter->second;
        }
    }

    std::unordered_map<std::string, std::string> valid_orders; 
    for (const auto& o : orders_data) {
        auto date_it = o.find("o_orderdate");
        auto cust_it = o.find("o_custkey");
        auto ord_it = o.find("o_orderkey");
        if (date_it != o.end() && date_it->second >= start_date && date_it->second < end_date) {
            if (cust_it != o.end() && ord_it != o.end() && cust_nation.count(cust_it->second)) {
                valid_orders[ord_it->second] = cust_it->second;
            }
        }
    }

    struct ThreadData {
        size_t start;
        size_t end;
        std::map<std::string, double>* local_res;
        const std::vector<std::map<std::string, std::string>>* lineitem_data;
        const std::unordered_map<std::string, std::string>* valid_orders;
        const std::unordered_map<std::string, std::string>* supp_nation;
        const std::unordered_map<std::string, std::string>* cust_nation;
        std::unordered_map<std::string, std::string>* valid_nations;
    };

    struct WorkerRunner {
        static DWORD WINAPI worker(LPVOID lpParam) {
            ThreadData* pData = (ThreadData*)lpParam;
            for (size_t i = pData->start; i < pData->end; ++i) {
                const auto& l = (*pData->lineitem_data)[i];
                auto ok_it = l.find("l_orderkey");
                auto sk_it = l.find("l_suppkey");
                auto ep_it = l.find("l_extendedprice");
                auto dc_it = l.find("l_discount");
                
                if (ok_it == l.end() || sk_it == l.end() || ep_it == l.end() || dc_it == l.end()) continue;

                auto o_it = pData->valid_orders->find(ok_it->second);
                if (o_it != pData->valid_orders->end()) {
                    const std::string& c_key = o_it->second;
                    auto s_it = pData->supp_nation->find(sk_it->second);
                    if (s_it != pData->supp_nation->end()) {
                        const std::string& sn_key = s_it->second;
                        const std::string& cn_key = pData->cust_nation->at(c_key); // Safe because logic ensures the key exists
                        if (sn_key == cn_key) {
                            double rev = std::stod(ep_it->second) * (1.0 - std::stod(dc_it->second));
                            (*pData->local_res)[(*pData->valid_nations)[sn_key]] += rev;
                        }
                    }
                }
            }
            return 0;
        }
    };

    std::vector<HANDLE> threads(num_threads);
    std::vector<ThreadData> thread_data(num_threads);
    std::vector<std::map<std::string, double>> local_results(num_threads);
    size_t chunk_size = lineitem_data.size() / num_threads;
    
    for (int i = 0; i < num_threads; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? lineitem_data.size() : start + chunk_size;
        thread_data[i] = {start, end, &local_results[i], &lineitem_data, &valid_orders, &supp_nation, &cust_nation, &valid_nations};
        threads[i] = CreateThread(NULL, 0, WorkerRunner::worker, &thread_data[i], 0, NULL);
    }

    WaitForMultipleObjects(num_threads, threads.data(), TRUE, INFINITE);
    for (int i = 0; i < num_threads; ++i) {
        CloseHandle(threads[i]);
    }

    for (const auto& lr : local_results) {
        for (const auto& kv : lr) {
            results[kv.first] += kv.second;
        }
    }

    return true;
}

// Function to output results to the specified path
bool outputResults(const std::string& result_path, const std::map<std::string, double>& results) {
    std::vector<std::pair<std::string, double>> sorted_res(results.begin(), results.end());
    std::sort(sorted_res.begin(), sorted_res.end(), [](const std::pair<std::string, double>& a, const std::pair<std::string, double>& b) {
        return a.second > b.second;
    });

    std::ofstream out(result_path);
    if (!out.is_open()) return false;
    out << "n_name|revenue\n";
    out << std::fixed << std::setprecision(4);
    for (const auto& p : sorted_res) {
        out << p.first << "|" << p.second << "\n";
    }
    return true;
}