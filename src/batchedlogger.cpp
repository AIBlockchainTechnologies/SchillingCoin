// Copyright (c) 2018-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <batchedlogger.h>

CBatchedLogger::CBatchedLogger(BCLog::LogFlags category, BCLog::Level level, const std::string& logging_function,
                               const std::string& source_file, int source_line) :
    m_accept{LogInstance().WillLogCategoryLevel(category, level)},
    m_category{category},
    m_level{level},
    m_logging_function{logging_function},
    m_source_file{source_file},
    m_source_line{source_line}
{
}

CBatchedLogger::~CBatchedLogger()
{
    Flush();
}

void CBatchedLogger::Flush()
{
    if (!m_accept || m_msg.empty()) {
        return;
    }

    util::log::Log(util::log::Entry{
        .category = m_category,
        .level = m_level,
        .should_ratelimit = false,
        .source_loc = SourceLocation{__func__},
        .message = std::move(m_msg),
    });
    m_msg.clear();
}