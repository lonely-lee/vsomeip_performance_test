// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <cstdio>

#include <sys/types.h>
#include <unistd.h>

class latency_measurer {
public:
    latency_measurer(std::uint32_t _pid);
    virtual ~latency_measurer();
    bool start();
    void stop();
    void measure();
    void print_result() const;
    void print_cpu_load() const;
    double get_cpu_load() const;
    void exit_measure();
    double get_cpu_load_period() const;

private:
    void get_now_time(timespec & _ts);
private:
    timespec before_,after_,diff_ts_;
    std::uint32_t payload_size_;
    std::uint32_t number_of_message_;
    std::uint64_t latency_us_;
    std::uint64_t throughput_;
    std::vector<std::uint64_t> latencys_us_;
    bool exit_flag_;
};


