﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RtmpPlayer_H_
#define SRC_RTMP_RtmpPlayer_H_

#include <memory>
#include <string>
#include <functional>
#include "amf.h"
#include "Rtmp.h"
#include "RtmpProtocol.h"
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Network/TcpClient.h"

using namespace toolkit;
using namespace mediakit::Client;

namespace mediakit {

//实现了rtmp播放器协议部分的功能，及数据接收功能
class RtmpPlayer : public PlayerBase, public TcpClient, public RtmpProtocol {
public:
    typedef std::shared_ptr<RtmpPlayer> Ptr;
    RtmpPlayer(const EventPoller::Ptr &poller);
    ~RtmpPlayer() override;

    void play(const string &strUrl) override;
    void pause(bool bPause) override;
    void teardown() override;

protected:
    virtual bool onCheckMeta(const AMFValue &val) = 0;
    virtual void onMediaData(const RtmpPacket::Ptr &chunk_data) = 0;
    uint32_t getProgressMilliSecond() const;
    void seekToMilliSecond(uint32_t ms);

protected:
    void onMediaData_l(const RtmpPacket::Ptr &chunk_data);
    //在获取config帧后才触发onPlayResult_l(而不是收到play命令回复)，所以此时所有track都初始化完毕了
    void onPlayResult_l(const SockException &ex, bool handshake_done);

    //form Tcpclient
    void onRecv(const Buffer::Ptr &buf) override;
    void onConnect(const SockException &err) override;
    void onErr(const SockException &ex) override;
    //from RtmpProtocol
    void onRtmpChunk(RtmpPacket &chunk_data) override;
    void onStreamDry(uint32_t stream_index) override;
    void onSendRawData(Buffer::Ptr buffer) override {
        send(std::move(buffer));
    }

    template<typename FUNC>
    void addOnResultCB(const FUNC &func) {
        lock_guard<recursive_mutex> lck(_mtx_on_result);
        _map_on_result.emplace(_send_req_id, func);
    }
    template<typename FUNC>
    void addOnStatusCB(const FUNC &func) {
        lock_guard<recursive_mutex> lck(_mtx_on_status);
        _deque_on_status.emplace_back(func);
    }

    void onCmd_result(AMFDecoder &dec);
    void onCmd_onStatus(AMFDecoder &dec);
    void onCmd_onMetaData(AMFDecoder &dec);

    void send_connect();
    void send_createStream();
    void send_play();
    void send_pause(bool pause);

private:
    string _app;
    string _stream_id;
    string _tc_url;

    bool _paused = false;
    bool _metadata_got = false;
    //是否为性能测试模式
    bool _benchmark_mode = false;

    //播放进度控制
    uint32_t _seek_ms = 0;
    uint32_t _fist_stamp[2] = {0, 0};
    uint32_t _now_stamp[2] = {0, 0};
    Ticker _now_stamp_ticker[2];

    recursive_mutex _mtx_on_result;
    recursive_mutex _mtx_on_status;
    deque<function<void(AMFValue &dec)> > _deque_on_status;
    unordered_map<int, function<void(AMFDecoder &dec)> > _map_on_result;

    //rtmp接收超时计时器
    Ticker _rtmp_recv_ticker;
    //心跳发送定时器
    std::shared_ptr<Timer> _beat_timer;
    //播放超时定时器
    std::shared_ptr<Timer> _play_timer;
    //rtmp接收超时定时器
    std::shared_ptr<Timer> _rtmp_recv_timer;
};

} /* namespace mediakit */
#endif /* SRC_RTMP_RtmpPlayer_H_ */
