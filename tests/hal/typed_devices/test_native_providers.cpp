/** Actual selected Native providers: binding, ownership and resource fences.
 * No vendor/physical timing is inferred from this host resource model. */
#include "hal/provider/nx_device_provider.h"
#include "hal/nx_hal.h"
#include "runtime/nx_runtime.h"
#include "arch/nx_arch.h"
#include <gtest/gtest.h>
#include <array>
extern "C" {
#include "../../../soc/native/private/native_platform.h"
void nx_isr_simulate(uint32_t irq);
}

class NativeProviders : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(nx_runtime_bootstrap(nullptr), NX_OK); }
    void TearDown() override {
        nx_device_shutdown_end();
        EXPECT_EQ(nx_runtime_shutdown(nullptr), NX_OK);
    }
};

TEST_F(NativeProviders, MaintainedDescriptorsUsePreciseConstructionAndStableBinding) {
    const std::array<std::pair<const char*, nx_device_class_t>,5> providers{{
        {"GPIOA0",NX_DEVICE_CLASS_GPIO}, {"UART0",NX_DEVICE_CLASS_UART},
        {"SPI0",NX_DEVICE_CLASS_SPI}, {"I2C0",NX_DEVICE_CLASS_I2C},
        {"FLASH0",NX_DEVICE_CLASS_FLASH}}};
    for (const auto& provider : providers) {
        const nx_device_t* descriptor=nullptr;
        ASSERT_EQ(nx_device_discover(provider.first,provider.second,&descriptor),NX_OK);
        ASSERT_NE(descriptor->construct,nullptr);
        EXPECT_EQ(descriptor->device_init,nullptr);
        void* rejected=reinterpret_cast<void*>(1);
        EXPECT_EQ(descriptor->construct(nullptr,&rejected),NX_ERR_INVALID_PARAM);
        EXPECT_EQ(rejected,nullptr);
        void* binding=nx_device_get_checked(provider.first,provider.second);
        ASSERT_NE(binding,nullptr);
        nx_lifecycle_t* life=descriptor->get_lifecycle ? descriptor->get_lifecycle(binding) :
            provider.second==NX_DEVICE_CLASS_GPIO ?
                static_cast<nx_gpio_t*>(binding)->write.get_lifecycle(&static_cast<nx_gpio_t*>(binding)->write) :
            provider.second==NX_DEVICE_CLASS_UART ? static_cast<nx_uart_t*>(binding)->get_lifecycle(static_cast<nx_uart_t*>(binding)) :
            provider.second==NX_DEVICE_CLASS_SPI ? static_cast<nx_spi_bus_t*>(binding)->get_lifecycle(static_cast<nx_spi_bus_t*>(binding)) :
            provider.second==NX_DEVICE_CLASS_I2C ? static_cast<nx_i2c_bus_t*>(binding)->get_lifecycle(static_cast<nx_i2c_bus_t*>(binding)) :
                static_cast<nx_internal_flash_t*>(binding)->get_lifecycle(static_cast<nx_internal_flash_t*>(binding));
        ASSERT_NE(life,nullptr);
        EXPECT_EQ(life->get_state(life),NX_DEV_STATE_UNINITIALIZED);
        EXPECT_EQ(nx_device_get_checked(provider.first,provider.second),binding);
    }
}
TEST_F(NativeProviders, GpioCloseReopenKeepsProviderBindingButRejectsOldOwner) {
    const nx_device_t* descriptor=nullptr;
    ASSERT_EQ(nx_device_discover("GPIOA0",NX_DEVICE_CLASS_GPIO,&descriptor),NX_OK);
    void* binding=nx_device_get_checked("GPIOA0",NX_DEVICE_CLASS_GPIO);
    nx_device_ref_t ref{};
    ASSERT_EQ(nx_device_open("GPIOA0",NX_DEVICE_CLASS_GPIO,1,&ref),NX_OK);
    EXPECT_EQ(nx_device_gpio_write(ref,1),NX_OK);
    EXPECT_EQ(nx_device_close(ref),NX_OK);
    nx_device_ref_t next{};
    ASSERT_EQ(nx_device_open("GPIOA0",NX_DEVICE_CLASS_GPIO,2,&next),NX_OK);
    EXPECT_EQ(descriptor->state->api,binding);
    EXPECT_NE(next.generation,ref.generation);
    EXPECT_EQ(nx_device_gpio_write(ref,0),NX_ERR_INVALID_STATE);
    EXPECT_EQ(nx_device_close(next),NX_OK);
}
TEST_F(NativeProviders, ShutdownFenceRejectsNewDmaAndInterruptOwners) {
    nx_dma_channel_t* dma=nx_dma_allocate_channel(0,0);
    ASSERT_NE(dma,nullptr);
    EXPECT_EQ(nx_native_resources_idle(),NX_ERR_BUSY);
    EXPECT_EQ(nx_dma_release_channel(dma),NX_OK);
    auto callback=[](void*) {};
    auto* manager=nx_isr_manager_get();
    ASSERT_EQ(manager->connect(manager,3,callback,nullptr,6),NX_OK);
    EXPECT_EQ(nx_native_resources_idle(),NX_ERR_BUSY);
    ASSERT_EQ(nx_device_shutdown_begin(),NX_OK);
    EXPECT_EQ(nx_dma_allocate_channel(0,0),nullptr);
    EXPECT_EQ(manager->connect(manager,4,callback,nullptr,6),NX_ERR_BUSY);
    EXPECT_EQ(manager->disconnect(manager,3),NX_OK);
    EXPECT_EQ(nx_native_resources_idle(),NX_OK);
    EXPECT_EQ(nx_device_provider_quiescence_check(),NX_OK);
}
TEST_F(NativeProviders, DmaCallbackPinsContextAndRunsOutsideMetadataMask) {
    struct Context { nx_dma_channel_t* channel; bool called=false; nx_status_t release=NX_OK; };
    nx_dma_channel_t* dma=nx_dma_allocate_channel(0,0);
    ASSERT_NE(dma,nullptr);
    Context context{dma};
    nx_dma_config_t cfg{}; cfg.size=4; cfg.data_width=1;
    ASSERT_EQ(dma->configure(dma,&cfg),NX_OK);
    auto callback=[](void* opaque) {
        auto* ctx=static_cast<Context*>(opaque);
        EXPECT_FALSE(nx_arch_irq_is_masked());
        ctx->called=true;
        ctx->release=nx_dma_release_channel(ctx->channel);
        EXPECT_EQ(nx_native_resources_idle(),NX_ERR_BUSY);
    };
    ASSERT_EQ(dma->set_callback(dma,callback,&context),NX_OK);
    EXPECT_EQ(dma->start(dma),NX_OK);
    EXPECT_TRUE(context.called);
    EXPECT_EQ(context.release,NX_ERR_BUSY);
    EXPECT_EQ(nx_dma_release_channel(dma),NX_OK);
    EXPECT_EQ(nx_native_resources_idle(),NX_OK);
}
