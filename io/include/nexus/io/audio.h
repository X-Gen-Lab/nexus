/**
 * \file            audio.h
 * \brief           Finite I2S transmit, receive and full duplex DMA loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_AUDIO_H
#define NEXUS_IO_AUDIO_H

#include "nexus/core/request.h"
#include "nexus/io/frame_stream.h"
#include "nexus/io/fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One finite block in the statically selected serial audio format.
 * \note Initialize/prepare base first. TX takes one exact immutable frame
 *       consumer loan; RX takes one FILLING loan from an idle framed stream.
 *       Full duplex requires both and equal complete frame storage lengths.
 *       Unselected directions must be NULL. Format conversion is never done.
 *       Sixteen bit samples use one native little endian halfword per slot;
 *       24/32 bit samples use HIGH halfword then LOW halfword, in DMA order.
 *       For 24 bit data the low halfword has eight trailing padding bits.
 *       This is a sequence of halfwords, not native little endian uint32 PCM.
 *       I2S/left/right justified blocks contain both channel slots; PCM blocks
 *       contain complete single channel synchronization periods.
 *       Data length determines stored halfwords; wire padding is not stored.
 *       Finite master RX requires at least two stored halfwords.
 *       ACCEPTED borrows this request and both controls until SETTLED. Only
 *       proved serializer, DMA, IRQ and address withdrawal returns TX or
 *       publishes RX. Error/cancel reports zero and aborts RX after drain.
 *       An external slave clock that stops while a word remains can prevent
 *       drain indefinitely; timeout and failed stop NEVER revoke the loan.
 */
typedef struct {
    nx_request_t base;
    nx_frame_dma_loan_t* tx;
    nx_frame_stream_t* rx;
} nx_audio_request_t;

/** \brief Exact rational nominal rate; external slave rate is unknown (0/0).
 * \note This is prescaler arithmetic, not a physical clock measurement.
 */
typedef struct {
    uint64_t numerator_hz;
    uint64_t denominator;
} nx_audio_rate_t;

/** \brief Allocation-free serialized owner, driven by explicit service.
 * \note Platform prepare acquires pads, DMA, I2S and an optional shared
 *       compatible PLLI2S lease. Factory lookup never opens hardware. Start
 *       external slave CK only after acquire-observed ACTIVE; WS must be at
 *       the selected idle channel boundary during preparation. ACTIVE in a
 *       slave TX mode includes a real DR prefill, three actual microsecond
 *       ticks and at least three peripheral clocks with idle CK/WS retained.
 *       Caller controls external electrical timing and stop/drain clocks.
 *       IRQs latch errors/completion, with no task creation. One privileged
 *       unmasked task serializes methods and resource lifecycle; explicit
 *       service progresses operations. Late IRQs cannot settle an old loan.
 *       stop cancels and joins the current block; failed drain retains full
 *       request/loan/resource storage. Platform close prevents further
 *       admission before attempting resource release. Join this owner before
 *       Platform stop or clock changes.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_audio_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*cancel)(void* context, nx_audio_request_t* request);
    nx_result_t (*stop)(void* context);
    nx_result_t (*rate)(void* context, nx_audio_rate_t* rate);
} nx_audio_ops_t;

/** \brief Immutable controller identity; vendor Handles remain private. */
struct nx_audio_port {
    const nx_audio_ops_t* ops;
    void* context;
};
/** \brief Same immutable typed controller face for direct operation use. */
typedef nx_audio_port_t nx_audio_t;

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_AUDIO_H */
