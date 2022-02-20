// [CFXS] //
#pragma once
#include "spdlog/sinks/base_sink.h"
#include <QString>
#include <QObject>
#include <vector>

class _L0_AppLogBufferSinkNotifier : public QObject {
    Q_OBJECT

public:
    static _L0_AppLogBufferSinkNotifier* GetInstance() {
        static _L0_AppLogBufferSinkNotifier n;
        return &n;
    }

    _L0_AppLogBufferSinkNotifier() = default;

signals:
    void Updated(const std::vector<QString>& buf, size_t bufferTotalSize); // Log updated
};

class L0_AppLogBufferSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    static _L0_AppLogBufferSinkNotifier* GetNotifier() {
        return _L0_AppLogBufferSinkNotifier::GetInstance();
    }

    static const std::vector<QString>& GetLogBuffer();
    static size_t GetBufferSize();
    static size_t GetEntryCount();

private:
    static std::vector<QString>& GetLogBufferRef();
    static size_t& GetBufferSizeRef();

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
        std::mutex m;
        m.lock();
        GetLogBufferRef().push_back(QString::fromStdString(fmt::to_string(formatted)));
        GetBufferSizeRef() += GetLogBufferRef().back().length() + 1;
        emit GetNotifier()->Updated(GetLogBufferRef(), GetBufferSize());
        m.unlock();
    }

    void flush_() override {
    }
};
