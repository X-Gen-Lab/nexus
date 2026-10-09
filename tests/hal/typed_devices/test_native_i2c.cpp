/** Actual Native I2C controller through the typed child facade; no MCU claim. */
#include "runtime/nx_runtime.h"
#include "hal/base/nx_device.h"
#include "hal/runtime/nx_deadline.h"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
extern "C" nx_status_t typed_i2c_response(nx_device_ref_t,uint8_t,const uint8_t*,size_t);
extern "C" nx_status_t typed_i2c_delay(nx_device_ref_t,uint32_t);
extern "C" nx_status_t typed_i2c_failure(nx_device_ref_t,nx_status_t);
class TypedNativeI2C : public ::testing::Test {
protected:
    nx_device_ref_t controller{};
    std::vector<nx_device_i2c_ref_t> children;
    uint8_t tx[2]{0x10,0x20},rx[4]{}; size_t received=0;
    nx_i2c_transaction_t transaction{tx,sizeof(tx),rx,sizeof(rx),&received,100,nullptr,nullptr};
    void SetUp() override {
        ASSERT_EQ(nx_runtime_bootstrap(nullptr),NX_OK);
        ASSERT_EQ(nx_device_open("I2C0",NX_DEVICE_CLASS_I2C,0x1c2,&controller),NX_OK);
        ASSERT_EQ(typed_i2c_delay(controller,0),NX_OK);
    }
    void TearDown() override {
        if(controller.descriptor) {
            for(unsigned tries=0;tries<3;++tries) { auto s=nx_device_i2c_service(controller); if(s==NX_ERR_NO_DATA) break; }
            for(auto ref:children) { auto s=nx_device_i2c_close(ref); EXPECT_TRUE(s==NX_OK || s==NX_ERR_INVALID_STATE); }
            EXPECT_EQ(nx_device_close(controller),NX_OK);
        }
        EXPECT_EQ(nx_runtime_shutdown(nullptr),NX_OK);
    }
    nx_device_i2c_ref_t child(uint8_t address=0x50) {
        nx_device_i2c_ref_t out{}; EXPECT_EQ(nx_device_i2c_open(controller,address,&out),NX_OK);
        if(out.slot) children.push_back(out);
        return out;
    }
};
TEST_F(TypedNativeI2C, DistinctAddressesConsumeOnlyTheirOwnInjectedResponse) {
    auto a=child(0x50),b=child(0x51);
    const uint8_t answerA[]{1,2},answerB[]{7,8,9};
    ASSERT_EQ(typed_i2c_response(controller,0x50,answerA,sizeof(answerA)),NX_OK);
    ASSERT_EQ(typed_i2c_response(controller,0x51,answerB,sizeof(answerB)),NX_OK);
    ASSERT_EQ(nx_device_i2c_transfer(b,&transaction),NX_OK); EXPECT_EQ(received,3u); EXPECT_EQ(rx[0],7);
    ASSERT_EQ(nx_device_i2c_transfer(a,&transaction),NX_OK); EXPECT_EQ(received,2u); EXPECT_EQ(rx[0],1);
    EXPECT_EQ(nx_device_close(controller),NX_ERR_BUSY);
}
TEST_F(TypedNativeI2C, InvalidAddressAndMalformedTransactionNeverAdmitIO) {
    nx_device_i2c_ref_t out{};
    EXPECT_EQ(nx_device_i2c_open(controller,128,&out),NX_ERR_INVALID_PARAM); EXPECT_EQ(out.slot,0u);
    auto ref=child(); transaction.tx_data=nullptr;
    EXPECT_EQ(nx_device_i2c_transfer(ref,&transaction),NX_ERR_INVALID_PARAM);
    transaction.tx_data=tx; transaction.timeout_ms=0;
    EXPECT_EQ(nx_device_i2c_transfer(ref,&transaction),NX_ERR_TIMEOUT);
}
TEST_F(TypedNativeI2C, PureReadKeepsReceivedLengthAndDoesNotRequireTXStorage) {
    auto ref=child(); const uint8_t answer[]{3,4,5};
    ASSERT_EQ(typed_i2c_response(controller,0x50,answer,sizeof(answer)),NX_OK);
    transaction.tx_data=nullptr; transaction.tx_length=0;
    ASSERT_EQ(nx_device_i2c_transfer(ref,&transaction),NX_OK);
    EXPECT_EQ(received,sizeof(answer)); EXPECT_EQ(std::memcmp(rx,answer,sizeof(answer)),0);
}
TEST_F(TypedNativeI2C, ChildPoolExhaustionFailsAndClosingReusesCapacitySafely) {
    for(unsigned i=0;i<NX_DEVICE_I2C_MAX_CHILDREN;++i) child(uint8_t(i));
    nx_device_i2c_ref_t rejected{};
    EXPECT_EQ(nx_device_i2c_open(controller,0x60,&rejected),NX_ERR_NO_RESOURCE);
    EXPECT_EQ(rejected.slot,0u);
    auto old=children.front(); ASSERT_EQ(nx_device_i2c_close(old),NX_OK);
    auto fresh=child(0x60); EXPECT_EQ(fresh.slot,old.slot); EXPECT_NE(fresh.generation,old.generation);
    EXPECT_EQ(nx_device_i2c_transfer(old,&transaction),NX_ERR_INVALID_STATE);
}
TEST_F(TypedNativeI2C, ReadsNeverManufactureEchoAndDeadlineIncludesResponseWait) {
    auto ref=child(); transaction.timeout_ms=5; received=123; rx[0]=0xee;
    EXPECT_EQ(nx_device_i2c_transfer(ref,&transaction),NX_ERR_TIMEOUT);
    EXPECT_EQ(received,0u); EXPECT_EQ(rx[0],0xee);
}
TEST_F(TypedNativeI2C, QueueDelayConsumesSubmitBudgetBeforeControllerService) {
    auto ref=child(); transaction.timeout_ms=5; nx_device_i2c_ticket_t ticket{};
    ASSERT_EQ(nx_device_i2c_submit(ref,&transaction,&ticket),NX_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(nx_device_i2c_close(ref),NX_ERR_BUSY);
    EXPECT_EQ(nx_device_i2c_service(controller),NX_ERR_TIMEOUT);
    nx_device_i2c_result_t result{}; ASSERT_EQ(nx_device_i2c_poll(ref,ticket,&result),NX_OK);
    EXPECT_TRUE(result.settled); EXPECT_EQ(result.status,NX_ERR_TIMEOUT);
}
TEST_F(TypedNativeI2C, CancelRequiresSettlementAndStaleTicketCannotCancelNext) {
    auto ref=child(); nx_device_i2c_ticket_t old{},next{};
    transaction.rx_capacity=0; transaction.rx_data=nullptr;
    ASSERT_EQ(nx_device_i2c_submit(ref,&transaction,&old),NX_OK);
    EXPECT_EQ(nx_device_i2c_cancel(ref,old),NX_OK); EXPECT_EQ(nx_device_i2c_close(ref),NX_ERR_BUSY);
    EXPECT_EQ(nx_device_i2c_service(controller),NX_ERR_CANCELLED);
    ASSERT_EQ(nx_device_i2c_submit(ref,&transaction,&next),NX_OK);
    EXPECT_EQ(nx_device_i2c_cancel(ref,old),NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_i2c_service(controller),NX_OK);
    nx_device_i2c_result_t result{}; EXPECT_EQ(nx_device_i2c_poll(ref,next,&result),NX_OK); EXPECT_TRUE(result.settled);
}
TEST_F(TypedNativeI2C, ChildCloseInvalidatesCopiesAcrossSlotReuse) {
    auto old=child(); ASSERT_EQ(nx_device_i2c_close(old),NX_OK); auto next=child();
    EXPECT_NE(old.generation,next.generation);
    EXPECT_EQ(nx_device_i2c_transfer(old,&transaction),NX_ERR_INVALID_STATE);
    transaction.rx_capacity=0; transaction.rx_data=nullptr;
    EXPECT_EQ(nx_device_i2c_transfer(next,&transaction),NX_OK);
}
struct CallbackContext { nx_device_i2c_ref_t ref; unsigned count=0; nx_status_t status=NX_OK,close=NX_OK; };
static void completed(void* data,nx_status_t status) {
    auto* c=static_cast<CallbackContext*>(data); ++c->count; c->status=status; c->close=nx_device_i2c_close(c->ref);
}
TEST_F(TypedNativeI2C, TerminalCallbackRunsOnceAndRetainsChildDuringDelivery) {
    auto ref=child(); CallbackContext context{ref}; transaction.callback=completed; transaction.user_data=&context;
    ASSERT_EQ(typed_i2c_failure(controller,NX_ERR_NACK),NX_OK);
    nx_device_i2c_ticket_t ticket{}; ASSERT_EQ(nx_device_i2c_submit(ref,&transaction,&ticket),NX_OK);
    EXPECT_EQ(nx_device_i2c_service(controller),NX_ERR_NACK);
    EXPECT_EQ(context.count,1u); EXPECT_EQ(context.status,NX_ERR_NACK); EXPECT_EQ(context.close,NX_ERR_BUSY);
    EXPECT_EQ(nx_device_i2c_service(controller),NX_ERR_NO_DATA); EXPECT_EQ(context.count,1u);
}
TEST_F(TypedNativeI2C, SynchronousCancellationFromSecondTaskWaitsForBufferSettlement) {
    auto ref=child(); ASSERT_EQ(typed_i2c_delay(controller,100),NX_OK); transaction.timeout_ms=500;
    std::atomic<bool> done{false}; nx_status_t result=NX_OK;
    std::thread worker([&]{ result=nx_device_i2c_transfer(ref,&transaction); done=true; });
    nx_status_t request=NX_ERR_NO_DATA; auto end=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(!done.load() && std::chrono::steady_clock::now()<end) {
        request=nx_device_i2c_cancel_transfer(ref); if(request==NX_OK) break;
        EXPECT_TRUE(request==NX_ERR_NO_DATA || request==NX_ERR_BUSY || request==NX_ERR_NOT_FOUND);
        std::this_thread::yield();
    }
    worker.join(); EXPECT_EQ(request,NX_OK); EXPECT_EQ(result,NX_ERR_CANCELLED); EXPECT_EQ(received,0u);
}
TEST_F(TypedNativeI2C, OSALWaitPortUsesRealTaskClockAndNoManufacturedProgress) {
    const nx_hal_wait_port_t* port=nx_hal_osal_wait_port();
    uint32_t before=0,after=0;
    ASSERT_EQ(port->now_ms(port->context,&before),NX_OK);
    ASSERT_EQ(port->wait_ms(port->context,2),NX_OK);
    ASSERT_EQ(port->now_ms(port->context,&after),NX_OK);
    EXPECT_GE(uint32_t(after-before),2u);
}
