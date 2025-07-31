#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("VAADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAADD(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x26862257);

    as.RewindBuffer();

    as.VAADD(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x24862257);
}

TEST_CASE("VAADD.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAADD(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2685E257);

    as.RewindBuffer();

    as.VAADD(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2485E257);
}

TEST_CASE("VAADDU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAADDU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x22862257);

    as.RewindBuffer();

    as.VAADDU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x20862257);
}

TEST_CASE("VAADDU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAADDU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2285E257);

    as.RewindBuffer();

    as.VAADDU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2085E257);
}

TEST_CASE("VADC.VVM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VADC(v4, v8, v12);
    REQUIRE(value == 0x40860257);
}

TEST_CASE("VADC.VXM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VADC(v4, v8, x11);
    REQUIRE(value == 0x4085C257);
}

TEST_CASE("VADC.VIM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VADC(v4, v8, 15);
    REQUIRE(value == 0x4087B257);

    as.RewindBuffer();

    as.VADC(v4, v8, -16);
    REQUIRE(value == 0x40883257);
}

TEST_CASE("VADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VADD(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x02860257);

    as.RewindBuffer();

    as.VADD(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x00860257);
}

TEST_CASE("VADD.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VADD(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x0285C257);

    as.RewindBuffer();

    as.VADD(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x0085C257);
}

TEST_CASE("VADD.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VADD(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x0287B257);

    as.RewindBuffer();

    as.VADD(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x02883257);

    as.RewindBuffer();

    as.VADD(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x0087B257);

    as.RewindBuffer();

    as.VADD(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x00883257);
}

TEST_CASE("VAND.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAND(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x26860257);

    as.RewindBuffer();

    as.VAND(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x24860257);
}

TEST_CASE("VAND.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAND(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2685C257);

    as.RewindBuffer();

    as.VAND(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2485C257);
}

TEST_CASE("VAND.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAND(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x2687B257);

    as.RewindBuffer();

    as.VAND(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x26883257);

    as.RewindBuffer();

    as.VAND(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x2487B257);

    as.RewindBuffer();

    as.VAND(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x24883257);
}

TEST_CASE("VASUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VASUB(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x2E862257);

    as.RewindBuffer();

    as.VASUB(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x2C862257);
}

TEST_CASE("VASUB.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VASUB(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2E85E257);

    as.RewindBuffer();

    as.VASUB(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2C85E257);
}

TEST_CASE("VASUBU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VASUBU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x2A862257);

    as.RewindBuffer();

    as.VASUBU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x28862257);
}

TEST_CASE("VASUBU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VASUBU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2A85E257);

    as.RewindBuffer();

    as.VASUBU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2885E257);
}

TEST_CASE("VCOMPRESS.VM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCOMPRESS(v4, v8, v12);
    REQUIRE(value == 0x5E862257);
}

TEST_CASE("VDIV.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VDIV(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x86862257);

    as.RewindBuffer();

    as.VDIV(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x84862257);
}

TEST_CASE("VDIV.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VDIV(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8685E257);

    as.RewindBuffer();

    as.VDIV(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8485E257);
}

TEST_CASE("VDIVU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VDIVU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x82862257);

    as.RewindBuffer();

    as.VDIVU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x80862257);
}

TEST_CASE("VDIVU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VDIVU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8285E257);

    as.RewindBuffer();

    as.VDIVU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8085E257);
}

TEST_CASE("VFADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFADD(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x02861257);

    as.RewindBuffer();

    as.VFADD(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x00861257);
}

TEST_CASE("VFADD.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFADD(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x02865257);

    as.RewindBuffer();

    as.VFADD(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x00865257);
}

TEST_CASE("VFCLASS.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCLASS(v4, v8, VecMask::No);
    REQUIRE(value == 0x4E881257);

    as.RewindBuffer();

    as.VFCLASS(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4C881257);
}

TEST_CASE("VFCVT.F.X.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCVT_F_X(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A819257);

    as.RewindBuffer();

    as.VFCVT_F_X(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48819257);
}

TEST_CASE("VFCVT.F.XU.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCVT_F_XU(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A811257);

    as.RewindBuffer();

    as.VFCVT_F_XU(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48811257);
}

TEST_CASE("VFCVT.RTZ.X.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCVT_RTZ_X_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A839257);

    as.RewindBuffer();

    as.VFCVT_RTZ_X_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48839257);
}

TEST_CASE("VFCVT.RTZ.XU.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCVT_RTZ_XU_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A831257);

    as.RewindBuffer();

    as.VFCVT_RTZ_XU_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48831257);
}

TEST_CASE("VFCVT.X.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCVT_X_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A809257);

    as.RewindBuffer();

    as.VFCVT_X_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48809257);
}

TEST_CASE("VFCVT.XU.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFCVT_XU_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A801257);

    as.RewindBuffer();

    as.VFCVT_XU_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48801257);
}

TEST_CASE("VFNCVT.F.F.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_F_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A8A1257);

    as.RewindBuffer();

    as.VFNCVT_F_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x488A1257);
}

TEST_CASE("VFNCVT.F.X.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_F_X(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A899257);

    as.RewindBuffer();

    as.VFNCVT_F_X(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48899257);
}

TEST_CASE("VFNCVT.F.XU.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_F_XU(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A891257);

    as.RewindBuffer();

    as.VFNCVT_F_XU(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48891257);
}

TEST_CASE("VFNCVT.ROD.F.F.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_ROD_F_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A8A9257);

    as.RewindBuffer();

    as.VFNCVT_ROD_F_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x488A9257);
}

TEST_CASE("VFNCVT.RTZ.X.F.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_RTZ_X_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A8B9257);

    as.RewindBuffer();

    as.VFNCVT_RTZ_X_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x488B9257);
}

TEST_CASE("VFNCVT.RTZ.XU.F.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_RTZ_XU_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A8B1257);

    as.RewindBuffer();

    as.VFNCVT_RTZ_XU_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x488B1257);
}

TEST_CASE("VFNCVT.X.F.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_X_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A889257);

    as.RewindBuffer();

    as.VFNCVT_X_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48889257);
}

TEST_CASE("VFNCVT.XU.F.W", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNCVT_XU_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A881257);

    as.RewindBuffer();

    as.VFNCVT_XU_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48881257);
}

TEST_CASE("VFWCVT.F.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_F_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A861257);

    as.RewindBuffer();

    as.VFWCVT_F_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48861257);
}

TEST_CASE("VFWCVT.F.X.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_F_X(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A859257);

    as.RewindBuffer();

    as.VFWCVT_F_X(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48859257);
}

TEST_CASE("VFWCVT.F.XU.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_F_XU(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A851257);

    as.RewindBuffer();

    as.VFWCVT_F_XU(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48851257);
}

TEST_CASE("VFWCVT.RTZ.X.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_RTZ_X_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A879257);

    as.RewindBuffer();

    as.VFWCVT_RTZ_X_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48879257);
}

TEST_CASE("VFWCVT.RTZ.XU.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_RTZ_XU_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A871257);

    as.RewindBuffer();

    as.VFWCVT_RTZ_XU_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48871257);
}

TEST_CASE("VFWCVT.X.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_X_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A849257);

    as.RewindBuffer();

    as.VFWCVT_X_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48849257);
}

TEST_CASE("VFWCVT.XU.F.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWCVT_XU_F(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A841257);

    as.RewindBuffer();

    as.VFWCVT_XU_F(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48841257);
}

TEST_CASE("VFDIV.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFDIV(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x82861257);

    as.RewindBuffer();

    as.VFDIV(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x80861257);
}

TEST_CASE("VFDIV.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFDIV(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x82865257);

    as.RewindBuffer();

    as.VFDIV(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x80865257);
}

TEST_CASE("VFRDIV.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFRDIV(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x86865257);

    as.RewindBuffer();

    as.VFRDIV(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x84865257);
}

TEST_CASE("VFIRST.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFIRST(x10, v12, VecMask::No);
    REQUIRE(value == 0x42C8A557);

    as.RewindBuffer();

    as.VFIRST(x10, v12, VecMask::Yes);
    REQUIRE(value == 0x40C8A557);
}

TEST_CASE("VFREDMAX.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFREDMAX(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x1E861257);

    as.RewindBuffer();

    as.VFREDMAX(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x1C861257);
}

TEST_CASE("VFREDMIN.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFREDMIN(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x16861257);

    as.RewindBuffer();

    as.VFREDMIN(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x14861257);
}

TEST_CASE("VFREDSUM.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFREDSUM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x06861257);

    as.RewindBuffer();

    as.VFREDSUM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x04861257);
}

TEST_CASE("VFREDOSUM.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFREDOSUM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x0E861257);

    as.RewindBuffer();

    as.VFREDOSUM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x0C861257);
}

TEST_CASE("VFMACC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMACC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xB2861257);

    as.RewindBuffer();

    as.VFMACC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xB0861257);
}

TEST_CASE("VFMACC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMACC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xB2865257);

    as.RewindBuffer();

    as.VFMACC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xB0865257);
}

TEST_CASE("VFMADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMADD(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xA2861257);

    as.RewindBuffer();

    as.VFMADD(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xA0861257);
}

TEST_CASE("VFMADD.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMADD(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xA2865257);

    as.RewindBuffer();

    as.VFMADD(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xA0865257);
}

TEST_CASE("VFMAX.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMAX(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x1A861257);

    as.RewindBuffer();

    as.VFMAX(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x18861257);
}

TEST_CASE("VFMAX.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMAX(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x1A865257);

    as.RewindBuffer();

    as.VFMAX(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x18865257);
}

TEST_CASE("VFMERGE.VFM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMERGE(v4, v8, f12);
    REQUIRE(value == 0x5C865257);
}

TEST_CASE("VFMIN.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMIN(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x12861257);

    as.RewindBuffer();

    as.VFMIN(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x10861257);
}

TEST_CASE("VFMIN.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMIN(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x12865257);

    as.RewindBuffer();

    as.VFMIN(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x10865257);
}

TEST_CASE("VFMSAC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMSAC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xBA861257);

    as.RewindBuffer();

    as.VFMSAC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xB8861257);
}

TEST_CASE("VFMSAC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMSAC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xBA865257);

    as.RewindBuffer();

    as.VFMSAC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xB8865257);
}

TEST_CASE("VFMSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMSUB(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xAA861257);

    as.RewindBuffer();

    as.VFMSUB(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xA8861257);
}

TEST_CASE("VFMSUB.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMSUB(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xAA865257);

    as.RewindBuffer();

    as.VFMSUB(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xA8865257);
}

TEST_CASE("VFMUL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMUL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x92861257);

    as.RewindBuffer();

    as.VFMUL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x90861257);
}

TEST_CASE("VFMUL.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMUL(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x92865257);

    as.RewindBuffer();

    as.VFMUL(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x90865257);
}

TEST_CASE("VFMV.F.S", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMV_FS(f10, v8);
    REQUIRE(value == 0x42801557);
}

TEST_CASE("VFMV.S.F", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMV_SF(v4, f11);
    REQUIRE(value == 0x4205D257);
}

TEST_CASE("VFMV.V.F", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFMV(v4, f11);
    REQUIRE(value == 0x5E05D257);
}

TEST_CASE("VFNMACC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMACC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xB6861257);

    as.RewindBuffer();

    as.VFNMACC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xB4861257);
}

TEST_CASE("VFNMACC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMACC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xB6865257);

    as.RewindBuffer();

    as.VFNMACC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xB4865257);
}

TEST_CASE("VFNMADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMADD(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xA6861257);

    as.RewindBuffer();

    as.VFNMADD(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xA4861257);
}

TEST_CASE("VFNMADD.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMADD(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xA6865257);

    as.RewindBuffer();

    as.VFNMADD(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xA4865257);
}

TEST_CASE("VFNMSAC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMSAC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xBE861257);

    as.RewindBuffer();

    as.VFNMSAC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xBC861257);
}

TEST_CASE("VFNMSAC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMSAC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xBE865257);

    as.RewindBuffer();

    as.VFNMSAC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xBC865257);
}

TEST_CASE("VFNMSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMSUB(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xAE861257);

    as.RewindBuffer();

    as.VFNMSUB(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xAC861257);
}

TEST_CASE("VFNMSUB.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFNMSUB(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xAE865257);

    as.RewindBuffer();

    as.VFNMSUB(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xAC865257);
}

TEST_CASE("VFREC7.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFREC7(v4, v8, VecMask::No);
    REQUIRE(value == 0x4E829257);

    as.RewindBuffer();

    as.VFREC7(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4C829257);
}

TEST_CASE("VFSGNJ.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSGNJ(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x22861257);

    as.RewindBuffer();

    as.VFSGNJ(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x20861257);
}

TEST_CASE("VFSGNJ.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSGNJ(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x22865257);

    as.RewindBuffer();

    as.VFSGNJ(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x20865257);
}

TEST_CASE("VFSGNJN.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSGNJN(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x26861257);

    as.RewindBuffer();

    as.VFSGNJN(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x24861257);
}

TEST_CASE("VFSGNJN.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSGNJN(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x26865257);

    as.RewindBuffer();

    as.VFSGNJN(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x24865257);
}

TEST_CASE("VFSGNJX.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSGNJX(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x2A861257);

    as.RewindBuffer();

    as.VFSGNJX(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x28861257);
}

TEST_CASE("VFSGNJX.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSGNJX(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x2A865257);

    as.RewindBuffer();

    as.VFSGNJX(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x28865257);
}

TEST_CASE("VFSQRT.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSQRT(v4, v8, VecMask::No);
    REQUIRE(value == 0x4E801257);

    as.RewindBuffer();

    as.VFSQRT(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4C801257);
}

TEST_CASE("VFRSQRT7.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFRSQRT7(v4, v8, VecMask::No);
    REQUIRE(value == 0x4E821257);

    as.RewindBuffer();

    as.VFRSQRT7(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4C821257);
}

TEST_CASE("VFSLIDE1DOWN.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSLIDE1DOWN(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x3E865257);

    as.RewindBuffer();

    as.VFSLIDE1DOWN(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x3C865257);
}

TEST_CASE("VFSLIDE1UP.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSLIDE1UP(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x3A865257);

    as.RewindBuffer();

    as.VFSLIDE1UP(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x38865257);
}

TEST_CASE("VFSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSUB(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x0A861257);

    as.RewindBuffer();

    as.VFSUB(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x08861257);
}

TEST_CASE("VFSUB.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFSUB(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x0A865257);

    as.RewindBuffer();

    as.VFSUB(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x08865257);
}

TEST_CASE("VFRSUB.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFRSUB(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x9E865257);

    as.RewindBuffer();

    as.VFRSUB(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x9C865257);
}

TEST_CASE("VFWADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWADD(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xC2861257);

    as.RewindBuffer();

    as.VFWADD(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC0861257);
}

TEST_CASE("VFWADD.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWADD(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0xC2865257);

    as.RewindBuffer();

    as.VFWADD(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0xC0865257);
}

TEST_CASE("VFWADD.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWADDW(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xD2861257);

    as.RewindBuffer();

    as.VFWADDW(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xD0861257);
}

TEST_CASE("VFWADD.WF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWADDW(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0xD2865257);

    as.RewindBuffer();

    as.VFWADDW(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0xD0865257);
}

TEST_CASE("VFWMACC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWMACC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xF2861257);

    as.RewindBuffer();

    as.VFWMACC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xF0861257);
}

TEST_CASE("VFWMACC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWMACC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xF2865257);

    as.RewindBuffer();

    as.VFWMACC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xF0865257);
}

TEST_CASE("VFWMUL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWMUL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xE2861257);

    as.RewindBuffer();

    as.VFWMUL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xE0861257);
}

TEST_CASE("VFWMUL.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWMUL(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0xE2865257);

    as.RewindBuffer();

    as.VFWMUL(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0xE0865257);
}

TEST_CASE("VFWNMACC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWNMACC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xF6861257);

    as.RewindBuffer();

    as.VFWNMACC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xF4861257);
}

TEST_CASE("VFWNMACC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWNMACC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xF6865257);

    as.RewindBuffer();

    as.VFWNMACC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xF4865257);
}

TEST_CASE("VFWNMSAC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWNMSAC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xFE861257);

    as.RewindBuffer();

    as.VFWNMSAC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xFC861257);
}

TEST_CASE("VFWNMSAC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWNMSAC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xFE865257);

    as.RewindBuffer();

    as.VFWNMSAC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xFC865257);
}

TEST_CASE("VFWREDSUM.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWREDSUM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xC6861257);

    as.RewindBuffer();

    as.VFWREDSUM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC4861257);
}

TEST_CASE("VFWREDOSUM.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWREDOSUM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xCE861257);

    as.RewindBuffer();

    as.VFWREDOSUM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xCC861257);
}

TEST_CASE("VFWMSAC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWMSAC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xFA861257);

    as.RewindBuffer();

    as.VFWMSAC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xF8861257);
}

TEST_CASE("VFWMSAC.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWMSAC(v4, f12, v8, VecMask::No);
    REQUIRE(value == 0xFA865257);

    as.RewindBuffer();

    as.VFWMSAC(v4, f12, v8, VecMask::Yes);
    REQUIRE(value == 0xF8865257);
}

TEST_CASE("VFWSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWSUB(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xCA861257);

    as.RewindBuffer();

    as.VFWSUB(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC8861257);
}

TEST_CASE("VFWSUB.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWSUB(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0xCA865257);

    as.RewindBuffer();

    as.VFWSUB(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0xC8865257);
}

TEST_CASE("VFWSUB.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWSUBW(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xDA861257);

    as.RewindBuffer();

    as.VFWSUBW(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xD8861257);
}

TEST_CASE("VFWSUB.WF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VFWSUBW(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0xDA865257);

    as.RewindBuffer();

    as.VFWSUBW(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0xD8865257);
}

TEST_CASE("VID.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VID(v4, VecMask::No);
    REQUIRE(value == 0x5208A257);

    as.RewindBuffer();

    as.VID(v4, VecMask::Yes);
    REQUIRE(value == 0x5008A257);
}

TEST_CASE("VIOTA.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VIOTA(v4, v8, VecMask::No);
    REQUIRE(value == 0x52882257);

    as.RewindBuffer();

    as.VIOTA(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x50882257);
}

TEST_CASE("VMACC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMACC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xB6862257);

    as.RewindBuffer();

    as.VMACC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xB4862257);
}

TEST_CASE("VMACC.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMACC(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xB685E257);

    as.RewindBuffer();

    as.VMACC(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xB485E257);
}

TEST_CASE("VMADC.VV(M)", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMADC(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x46860257);

    as.RewindBuffer();

    as.VMADC(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x44860257);
}

TEST_CASE("VMADC.VX(M)", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMADC(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x4685C257);

    as.RewindBuffer();

    as.VMADC(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x4485C257);
}

TEST_CASE("VMADC.VI(M)", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMADC(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x4687B257);

    as.RewindBuffer();

    as.VMADC(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x46883257);

    as.RewindBuffer();

    as.VMADC(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x4487B257);

    as.RewindBuffer();

    as.VMADC(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x44883257);
}

TEST_CASE("VMADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMADD(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xA6862257);

    as.RewindBuffer();

    as.VMADD(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xA4862257);
}

TEST_CASE("VMADD.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMADD(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xA685E257);

    as.RewindBuffer();

    as.VMADD(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xA485E257);
}

TEST_CASE("VMAND.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMAND(v4, v8, v12);
    REQUIRE(value == 0x66862257);

    as.RewindBuffer();

    as.VMMV(v4, v9);
    REQUIRE(value == 0x6694A257);
}

TEST_CASE("VMANDNOT.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMANDNOT(v4, v8, v12);
    REQUIRE(value == 0x62862257);
}

TEST_CASE("VMFEQ.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFEQ(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x62861257);

    as.RewindBuffer();

    as.VMFEQ(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x60861257);
}

TEST_CASE("VMFEQ.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFEQ(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x62865257);

    as.RewindBuffer();

    as.VMFEQ(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x60865257);
}

TEST_CASE("VMFGE.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFGE(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x7E865257);

    as.RewindBuffer();

    as.VMFGE(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x7C865257);
}

TEST_CASE("VMFGT.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFGT(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x76865257);

    as.RewindBuffer();

    as.VMFGT(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x74865257);
}

TEST_CASE("VMFLE.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFLE(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x66861257);

    as.RewindBuffer();

    as.VMFLE(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x64861257);
}

TEST_CASE("VMFLE.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFLE(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x66865257);

    as.RewindBuffer();

    as.VMFLE(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x64865257);
}

TEST_CASE("VMFLT.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFLT(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x6E861257);

    as.RewindBuffer();

    as.VMFLT(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x6C861257);
}

TEST_CASE("VMFLT.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFLT(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x6E865257);

    as.RewindBuffer();

    as.VMFLT(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x6C865257);
}

TEST_CASE("VMFNE.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFNE(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x72861257);

    as.RewindBuffer();

    as.VMFNE(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x70861257);
}

TEST_CASE("VMFNE.VF", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMFNE(v4, v8, f12, VecMask::No);
    REQUIRE(value == 0x72865257);

    as.RewindBuffer();

    as.VMFNE(v4, v8, f12, VecMask::Yes);
    REQUIRE(value == 0x70865257);
}

TEST_CASE("VMNAND.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMNAND(v4, v8, v12);
    REQUIRE(value == 0x76862257);

    as.RewindBuffer();

    as.VMNOT(v4, v9);
    REQUIRE(value == 0x7694A257);
}

TEST_CASE("VMNOR.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMNOR(v4, v8, v12);
    REQUIRE(value == 0x7A862257);
}

TEST_CASE("VMOR.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMOR(v4, v8, v12);
    REQUIRE(value == 0x6A862257);
}

TEST_CASE("VMORNOT.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMORNOT(v4, v8, v12);
    REQUIRE(value == 0x72862257);
}

TEST_CASE("VMXNOR.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMXNOR(v4, v8, v12);
    REQUIRE(value == 0x7E862257);

    as.RewindBuffer();

    as.VMSET(v9);
    REQUIRE(value == 0x7E94A4D7);
}

TEST_CASE("VMXOR.MM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMXOR(v4, v8, v12);
    REQUIRE(value == 0x6E862257);

    as.RewindBuffer();

    as.VMCLR(v9);
    REQUIRE(value == 0x6E94A4D7);
}

TEST_CASE("VMAX.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMAX(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x1E860257);

    as.RewindBuffer();

    as.VMAX(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x1C860257);
}

TEST_CASE("VMAX.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMAX(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x1E85C257);

    as.RewindBuffer();

    as.VMAX(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x1C85C257);
}

TEST_CASE("VMAXU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMAXU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x1A860257);

    as.RewindBuffer();

    as.VMAXU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x18860257);
}

TEST_CASE("VMAXU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMAXU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x1A85C257);

    as.RewindBuffer();

    as.VMAXU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x1885C257);
}

TEST_CASE("VMERGE.VVM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMERGE(v4, v8, v12);
    REQUIRE(value == 0x5C860257);
}

TEST_CASE("VMERGE.VXM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMERGE(v4, v8, x11);
    REQUIRE(value == 0x5C85C257);
}

TEST_CASE("VMERGE.VIM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMERGE(v4, v8, 15);
    REQUIRE(value == 0x5C87B257);

    as.RewindBuffer();

    as.VMERGE(v4, v8, -16);
    REQUIRE(value == 0x5C883257);
}

TEST_CASE("VMIN.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMIN(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x16860257);

    as.RewindBuffer();

    as.VMIN(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x14860257);
}

TEST_CASE("VMIN.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMIN(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x1685C257);

    as.RewindBuffer();

    as.VMIN(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x1485C257);
}

TEST_CASE("VMINU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMINU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x12860257);

    as.RewindBuffer();

    as.VMINU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x10860257);
}

TEST_CASE("VMINU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMINU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x1285C257);

    as.RewindBuffer();

    as.VMINU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x1085C257);
}

TEST_CASE("VMSBC.VV(M)", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSBC(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x4E860257);

    as.RewindBuffer();

    as.VMSBC(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x4C860257);
}

TEST_CASE("VMSBC.VX(M)", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSBC(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x4E85C257);

    as.RewindBuffer();

    as.VMSBC(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x4C85C257);
}

TEST_CASE("VMSBF.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSBF(v4, v8, VecMask::No);
    REQUIRE(value == 0x5280A257);

    as.RewindBuffer();

    as.VMSBF(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x5080A257);
}

TEST_CASE("VMSIF.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSIF(v4, v8, VecMask::No);
    REQUIRE(value == 0x5281A257);

    as.RewindBuffer();

    as.VMSIF(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x5081A257);
}

TEST_CASE("VMSOF.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSOF(v4, v8, VecMask::No);
    REQUIRE(value == 0x52812257);

    as.RewindBuffer();

    as.VMSOF(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x50812257);
}

TEST_CASE("VMSEQ.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSEQ(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x62860257);

    as.RewindBuffer();

    as.VMSEQ(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x60860257);
}

TEST_CASE("VMSEQ.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSEQ(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x6285C257);

    as.RewindBuffer();

    as.VMSEQ(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x6085C257);
}

TEST_CASE("VMSEQ.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSEQ(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x6287B257);

    as.RewindBuffer();

    as.VMSEQ(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x62883257);

    as.RewindBuffer();

    as.VMSEQ(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x6087B257);

    as.RewindBuffer();

    as.VMSEQ(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x60883257);
}

TEST_CASE("VMSGT.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSGT(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x7E85C257);

    as.RewindBuffer();

    as.VMSGT(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x7C85C257);
}

TEST_CASE("VMSGT.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSGT(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x7E87B257);

    as.RewindBuffer();

    as.VMSGT(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x7E883257);

    as.RewindBuffer();

    as.VMSGT(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x7C87B257);

    as.RewindBuffer();

    as.VMSGT(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x7C883257);

    as.RewindBuffer();

    as.VMSGE(v4, v8, 16, VecMask::No);
    REQUIRE(value == 0x7E87B257);
}

TEST_CASE("VMSGTU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSGTU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x7A85C257);

    as.RewindBuffer();

    as.VMSGTU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x7885C257);
}

TEST_CASE("VMSGTU.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSGTU(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x7A87B257);

    as.RewindBuffer();

    as.VMSGTU(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x7A883257);

    as.RewindBuffer();

    as.VMSGTU(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x7887B257);

    as.RewindBuffer();

    as.VMSGTU(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x78883257);

    as.RewindBuffer();

    as.VMSGEU(v4, v8, 16, VecMask::No);
    REQUIRE(value == 0x7A87B257);
}

TEST_CASE("VMSLE.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLE(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x76860257);

    as.RewindBuffer();

    as.VMSLE(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x74860257);

    as.RewindBuffer();

    as.VMSGE(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x76C40257);
}

TEST_CASE("VMSLE.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLE(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x7685C257);

    as.RewindBuffer();

    as.VMSLE(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x7485C257);
}

TEST_CASE("VMSLE.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLE(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x7687B257);

    as.RewindBuffer();

    as.VMSLE(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x76883257);

    as.RewindBuffer();

    as.VMSLE(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x7487B257);

    as.RewindBuffer();

    as.VMSLE(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x74883257);

    as.RewindBuffer();

    as.VMSLT(v4, v8, -15, VecMask::No);
    REQUIRE(value == 0x76883257);
}

TEST_CASE("VMSLEU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLEU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x72860257);

    as.RewindBuffer();

    as.VMSLEU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x70860257);

    as.RewindBuffer();

    as.VMSGEU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x72C40257);
}

TEST_CASE("VMSLEU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLEU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x7285C257);

    as.RewindBuffer();

    as.VMSLEU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x7085C257);
}

TEST_CASE("VMSLEU.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLEU(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x7287B257);

    as.RewindBuffer();

    as.VMSLEU(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x72883257);

    as.RewindBuffer();

    as.VMSLEU(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x7087B257);

    as.RewindBuffer();

    as.VMSLEU(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x70883257);

    as.RewindBuffer();

    as.VMSLTU(v4, v8, 16, VecMask::No);
    REQUIRE(value == 0x7287B257);
}

TEST_CASE("VMSLT.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLT(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x6E860257);

    as.RewindBuffer();

    as.VMSLT(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x6C860257);

    as.RewindBuffer();

    as.VMSGT(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x6EC40257);
}

TEST_CASE("VMSLT.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLT(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x6E85C257);

    as.RewindBuffer();

    as.VMSLT(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x6C85C257);
}

TEST_CASE("VMSLTU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLTU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x6A860257);

    as.RewindBuffer();

    as.VMSLTU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x68860257);

    as.RewindBuffer();

    as.VMSGTU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x6AC40257);
}

TEST_CASE("VMSLTU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSLTU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x6A85C257);

    as.RewindBuffer();

    as.VMSLTU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x6885C257);
}

TEST_CASE("VMSNE.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSNE(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x66860257);

    as.RewindBuffer();

    as.VMSNE(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x64860257);
}

TEST_CASE("VMSNE.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSNE(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x6685C257);

    as.RewindBuffer();

    as.VMSNE(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x6485C257);
}

TEST_CASE("VMSNE.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMSNE(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x6687B257);

    as.RewindBuffer();

    as.VMSNE(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x66883257);

    as.RewindBuffer();

    as.VMSNE(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x6487B257);

    as.RewindBuffer();

    as.VMSNE(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x64883257);
}

TEST_CASE("VMUL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMUL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x96862257);

    as.RewindBuffer();

    as.VMUL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x94862257);
}

TEST_CASE("VMUL.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMUL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x9685E257);

    as.RewindBuffer();

    as.VMUL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x9485E257);
}

TEST_CASE("VMULH.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMULH(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x9E862257);

    as.RewindBuffer();

    as.VMULH(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x9C862257);
}

TEST_CASE("VMULH.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMULH(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x9E85E257);

    as.RewindBuffer();

    as.VMULH(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x9C85E257);
}

TEST_CASE("VMULHSU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMULHSU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x9A862257);

    as.RewindBuffer();

    as.VMULHSU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x98862257);
}

TEST_CASE("VMULHSU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMULHSU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x9A85E257);

    as.RewindBuffer();

    as.VMULHSU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x9885E257);
}

TEST_CASE("VMULHU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMULHU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x92862257);

    as.RewindBuffer();

    as.VMULHU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x90862257);
}

TEST_CASE("VMULHU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMULHU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x9285E257);

    as.RewindBuffer();

    as.VMULHU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x9085E257);
}

TEST_CASE("VMV.S.X", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV_SX(v4, x10);
    REQUIRE(value == 0x42056257);
}

TEST_CASE("VMV.X.S", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV_XS(x10, v12);
    REQUIRE(value == 0x42C02557);
}

TEST_CASE("VMV.V.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV(v8, v12);
    REQUIRE(value == 0x5E060457);
}

TEST_CASE("VMV.V.X", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV(v8, x11);
    REQUIRE(value == 0x5E05C457);
}

TEST_CASE("VMV.V.I", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV(v8, 15);
    REQUIRE(value == 0x5E07B457);

    as.RewindBuffer();

    as.VMV(v8, -16);
    REQUIRE(value == 0x5E083457);
}

TEST_CASE("VMV1R.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV1R(v1, v2);
    REQUIRE(value == 0x9E2030D7);
}

TEST_CASE("VMV2R.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV2R(v2, v4);
    REQUIRE(value == 0x9E40B157);
}

TEST_CASE("VMV4R.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV4R(v4, v8);
    REQUIRE(value == 0x9E81B257);
}

TEST_CASE("VMV8R.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VMV8R(v0, v8);
    REQUIRE(value == 0x9E83B057);
}

TEST_CASE("VNCLIP.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNCLIP(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xBE860257);

    as.RewindBuffer();

    as.VNCLIP(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xBC860257);
}

TEST_CASE("VNCLIP.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNCLIP(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xBE85C257);

    as.RewindBuffer();

    as.VNCLIP(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xBC85C257);
}

TEST_CASE("VNCLIP.WI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNCLIP(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xBE80B257);

    as.RewindBuffer();

    as.VNCLIP(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xBE8FB257);

    as.RewindBuffer();

    as.VNCLIP(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xBC80B257);

    as.RewindBuffer();

    as.VNCLIP(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xBC8FB257);
}

TEST_CASE("VNCLIPU.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNCLIPU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xBA860257);

    as.RewindBuffer();

    as.VNCLIPU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xB8860257);
}

TEST_CASE("VNCLIPU.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNCLIPU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xBA85C257);

    as.RewindBuffer();

    as.VNCLIPU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xB885C257);
}

TEST_CASE("VNCLIPU.WI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNCLIPU(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xBA80B257);

    as.RewindBuffer();

    as.VNCLIPU(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xBA8FB257);

    as.RewindBuffer();

    as.VNCLIPU(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xB880B257);

    as.RewindBuffer();

    as.VNCLIPU(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xB88FB257);
}

TEST_CASE("VNMSAC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNMSAC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xBE862257);

    as.RewindBuffer();

    as.VNMSAC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xBC862257);
}

TEST_CASE("VNMSAC.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNMSAC(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xBE85E257);

    as.RewindBuffer();

    as.VNMSAC(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xBC85E257);
}

TEST_CASE("VNMSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNMSUB(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xAE862257);

    as.RewindBuffer();

    as.VNMSUB(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xAC862257);
}

TEST_CASE("VNMSUB.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNMSUB(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xAE85E257);

    as.RewindBuffer();

    as.VNMSUB(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xAC85E257);
}

TEST_CASE("VNSRA.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNSRA(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xB6860257);

    as.RewindBuffer();

    as.VNSRA(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xB4860257);
}

TEST_CASE("VNSRA.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNSRA(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xB685C257);

    as.RewindBuffer();

    as.VNSRA(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xB485C257);
}

TEST_CASE("VNSRA.WI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNSRA(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xB680B257);

    as.RewindBuffer();

    as.VNSRA(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xB68FB257);

    as.RewindBuffer();

    as.VNSRA(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xB480B257);

    as.RewindBuffer();

    as.VNSRA(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xB48FB257);
}

TEST_CASE("VNSRL.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNSRL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xB2860257);

    as.RewindBuffer();

    as.VNSRL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xB0860257);
}

TEST_CASE("VNSRL.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNSRL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xB285C257);

    as.RewindBuffer();

    as.VNSRL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xB085C257);

    as.RewindBuffer();

    as.VNCVT(v4, v8, VecMask::Yes);
    REQUIRE(value == 0xB0804257);
}

TEST_CASE("VNSRL.WI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VNSRL(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xB280B257);

    as.RewindBuffer();

    as.VNSRL(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xB28FB257);

    as.RewindBuffer();

    as.VNSRL(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xB080B257);

    as.RewindBuffer();

    as.VNSRL(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xB08FB257);
}

TEST_CASE("VOR.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VOR(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x2A860257);

    as.RewindBuffer();

    as.VOR(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x28860257);
}

TEST_CASE("VOR.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VOR(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2A85C257);

    as.RewindBuffer();

    as.VOR(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2885C257);
}

TEST_CASE("VOR.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VOR(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x2A87B257);

    as.RewindBuffer();

    as.VOR(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x2A883257);

    as.RewindBuffer();

    as.VOR(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x2887B257);

    as.RewindBuffer();

    as.VOR(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x28883257);
}

TEST_CASE("VPOPC.M", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VPOPC(x10, v12, VecMask::No);
    REQUIRE(value == 0x42C82557);

    as.RewindBuffer();

    as.VPOPC(x10, v12, VecMask::Yes);
    REQUIRE(value == 0x40C82557);
}

TEST_CASE("VREDAND.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDAND(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x06862257);

    as.RewindBuffer();

    as.VREDAND(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x04862257);
}

TEST_CASE("VREDMAX.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDMAX(v4, v8, v8, VecMask::No);
    REQUIRE(value == 0x1E842257);

    as.RewindBuffer();

    as.VREDMAX(v4, v8, v8, VecMask::Yes);
    REQUIRE(value == 0x1C842257);
}

TEST_CASE("VREDMAXU.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDMAXU(v4, v8, v8, VecMask::No);
    REQUIRE(value == 0x1A842257);

    as.RewindBuffer();

    as.VREDMAXU(v4, v8, v8, VecMask::Yes);
    REQUIRE(value == 0x18842257);
}

TEST_CASE("VREDMIN.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDMIN(v4, v8, v8, VecMask::No);
    REQUIRE(value == 0x16842257);

    as.RewindBuffer();

    as.VREDMIN(v4, v8, v8, VecMask::Yes);
    REQUIRE(value == 0x14842257);
}

TEST_CASE("VREDMINU.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDMINU(v4, v8, v8, VecMask::No);
    REQUIRE(value == 0x12842257);

    as.RewindBuffer();

    as.VREDMINU(v4, v8, v8, VecMask::Yes);
    REQUIRE(value == 0x10842257);
}

TEST_CASE("VREDOR.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDOR(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x0A862257);

    as.RewindBuffer();

    as.VREDOR(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x08862257);
}

TEST_CASE("VREDSUM.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDSUM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x02862257);

    as.RewindBuffer();

    as.VREDSUM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x00862257);
}

TEST_CASE("VREDXOR.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREDXOR(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x0E862257);

    as.RewindBuffer();

    as.VREDXOR(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x0C862257);
}

TEST_CASE("VREM.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x8E862257);

    as.RewindBuffer();

    as.VREM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x8C862257);
}

TEST_CASE("VREM.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREM(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8E85E257);

    as.RewindBuffer();

    as.VREM(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8C85E257);
}

TEST_CASE("VREMU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREMU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x8A862257);

    as.RewindBuffer();

    as.VREMU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x88862257);
}

TEST_CASE("VREMU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREMU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8A85E257);

    as.RewindBuffer();

    as.VREMU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8885E257);
}

TEST_CASE("VRGATHER.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VRGATHER(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x32860257);

    as.RewindBuffer();

    as.VRGATHER(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x30860257);
}

TEST_CASE("VRGATHER.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VRGATHER(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x3285C257);

    as.RewindBuffer();

    as.VRGATHER(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x3085C257);
}

TEST_CASE("VRGATHER.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VRGATHER(v4, v8, 0, VecMask::No);
    REQUIRE(value == 0x32803257);

    as.RewindBuffer();

    as.VRGATHER(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0x328FB257);

    as.RewindBuffer();

    as.VRGATHER(v4, v8, 0, VecMask::Yes);
    REQUIRE(value == 0x30803257);

    as.RewindBuffer();

    as.VRGATHER(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0x308FB257);
}

TEST_CASE("VRGATHEREI16.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VRGATHEREI16(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x3A860257);

    as.RewindBuffer();

    as.VRGATHEREI16(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x38860257);
}

TEST_CASE("VRSUB.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VRSUB(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x0E85C257);

    as.RewindBuffer();

    as.VRSUB(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x0C85C257);
}

TEST_CASE("VRSUB.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VRSUB(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x0E87B257);

    as.RewindBuffer();

    as.VRSUB(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x0E883257);

    as.RewindBuffer();

    as.VRSUB(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x0C87B257);

    as.RewindBuffer();

    as.VRSUB(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x0C883257);
}

TEST_CASE("VSADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSADD(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x86860257);

    as.RewindBuffer();

    as.VSADD(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x84860257);
}

TEST_CASE("VSADD.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSADD(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8685C257);

    as.RewindBuffer();

    as.VSADD(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8485C257);
}

TEST_CASE("VSADD.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSADD(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x8687B257);

    as.RewindBuffer();

    as.VSADD(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x86883257);

    as.RewindBuffer();

    as.VSADD(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x8487B257);

    as.RewindBuffer();

    as.VSADD(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x84883257);
}

TEST_CASE("VSADDU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSADDU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x82860257);

    as.RewindBuffer();

    as.VSADDU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x80860257);
}

TEST_CASE("VSADDU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSADDU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8285C257);

    as.RewindBuffer();

    as.VSADDU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8085C257);
}

TEST_CASE("VSADDU.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSADDU(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x8287B257);

    as.RewindBuffer();

    as.VSADDU(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x82883257);

    as.RewindBuffer();

    as.VSADDU(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x8087B257);

    as.RewindBuffer();

    as.VSADDU(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x80883257);
}

TEST_CASE("VSBC.VVM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSBC(v4, v8, v12);
    REQUIRE(value == 0x48860257);
}

TEST_CASE("VSBC.VXM", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSBC(v4, v8, x11);
    REQUIRE(value == 0x4885C257);
}

TEST_CASE("VSEXT.VF2", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSEXTVF2(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A83A257);

    as.RewindBuffer();

    as.VSEXTVF2(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4883A257);
}

TEST_CASE("VSEXT.VF4", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSEXTVF4(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A82A257);

    as.RewindBuffer();

    as.VSEXTVF4(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4882A257);
}

TEST_CASE("VSEXT.VF8", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSEXTVF8(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A81A257);

    as.RewindBuffer();

    as.VSEXTVF8(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x4881A257);
}

TEST_CASE("VSLIDE1DOWN.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLIDE1DOWN(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x3E85E257);

    as.RewindBuffer();

    as.VSLIDE1DOWN(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x3C85E257);
}

TEST_CASE("VSLIDEDOWN.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLIDEDOWN(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x3E85C257);

    as.RewindBuffer();

    as.VSLIDEDOWN(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x3C85C257);
}

TEST_CASE("VSLIDEDOWN.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLIDEDOWN(v4, v8, 0, VecMask::No);
    REQUIRE(value == 0x3E803257);

    as.RewindBuffer();

    as.VSLIDEDOWN(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0x3E8FB257);

    as.RewindBuffer();

    as.VSLIDEDOWN(v4, v8, 0, VecMask::Yes);
    REQUIRE(value == 0x3C803257);

    as.RewindBuffer();

    as.VSLIDEDOWN(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0x3C8FB257);
}

TEST_CASE("VSLIDE1UP.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLIDE1UP(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x3A85E257);

    as.RewindBuffer();

    as.VSLIDE1UP(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x3885E257);
}

TEST_CASE("VSLIDEUP.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLIDEUP(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x3A85C257);

    as.RewindBuffer();

    as.VSLIDEUP(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x3885C257);
}

TEST_CASE("VSLIDEUP.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLIDEUP(v4, v8, 0, VecMask::No);
    REQUIRE(value == 0x3A803257);

    as.RewindBuffer();

    as.VSLIDEUP(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0x3A8FB257);

    as.RewindBuffer();

    as.VSLIDEUP(v4, v8, 0, VecMask::Yes);
    REQUIRE(value == 0x38803257);

    as.RewindBuffer();

    as.VSLIDEUP(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0x388FB257);
}

TEST_CASE("VSLL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x96860257);

    as.RewindBuffer();

    as.VSLL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x94860257);
}

TEST_CASE("VSLL.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x9685C257);

    as.RewindBuffer();

    as.VSLL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x9485C257);
}

TEST_CASE("VSLL.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSLL(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0x9680B257);

    as.RewindBuffer();

    as.VSLL(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0x968FB257);

    as.RewindBuffer();

    as.VSLL(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0x9480B257);

    as.RewindBuffer();

    as.VSLL(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0x948FB257);
}

TEST_CASE("VSMUL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSMUL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x9E860257);

    as.RewindBuffer();

    as.VSMUL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x9C860257);
}

TEST_CASE("VSMUL.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSMUL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x9E85C257);

    as.RewindBuffer();

    as.VSMUL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x9C85C257);
}

TEST_CASE("VSRA.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSRA(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xA6860257);

    as.RewindBuffer();

    as.VSRA(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xA4860257);
}

TEST_CASE("VSRA.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSRA(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xA685C257);

    as.RewindBuffer();

    as.VSRA(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xA485C257);
}

TEST_CASE("VSRA.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSRA(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xA680B257);

    as.RewindBuffer();

    as.VSRA(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xA68FB257);

    as.RewindBuffer();

    as.VSRA(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xA480B257);

    as.RewindBuffer();

    as.VSRA(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xA48FB257);
}

TEST_CASE("VSRL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSRL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xA2860257);

    as.RewindBuffer();

    as.VSRL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xA0860257);
}

TEST_CASE("VSRL.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSRL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xA285C257);

    as.RewindBuffer();

    as.VSRL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xA085C257);
}

TEST_CASE("VSRL.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSRL(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xA280B257);

    as.RewindBuffer();

    as.VSRL(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xA28FB257);

    as.RewindBuffer();

    as.VSRL(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xA080B257);

    as.RewindBuffer();

    as.VSRL(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xA08FB257);
}

TEST_CASE("VSSRA.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSRA(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xAE860257);

    as.RewindBuffer();

    as.VSSRA(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xAC860257);
}

TEST_CASE("VSSRA.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSRA(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xAE85C257);

    as.RewindBuffer();

    as.VSSRA(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xAC85C257);
}

TEST_CASE("VSSRA.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSRA(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xAE80B257);

    as.RewindBuffer();

    as.VSSRA(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xAE8FB257);

    as.RewindBuffer();

    as.VSSRA(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xAC80B257);

    as.RewindBuffer();

    as.VSSRA(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xAC8FB257);
}

TEST_CASE("VSSRL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSRL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xAA860257);

    as.RewindBuffer();

    as.VSSRL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xA8860257);
}

TEST_CASE("VSSRL.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSRL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xAA85C257);

    as.RewindBuffer();

    as.VSSRL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xA885C257);
}

TEST_CASE("VSSRL.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSRL(v4, v8, 1, VecMask::No);
    REQUIRE(value == 0xAA80B257);

    as.RewindBuffer();

    as.VSSRL(v4, v8, 31, VecMask::No);
    REQUIRE(value == 0xAA8FB257);

    as.RewindBuffer();

    as.VSSRL(v4, v8, 1, VecMask::Yes);
    REQUIRE(value == 0xA880B257);

    as.RewindBuffer();

    as.VSSRL(v4, v8, 31, VecMask::Yes);
    REQUIRE(value == 0xA88FB257);
}

TEST_CASE("VSSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSUB(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x8E860257);

    as.RewindBuffer();

    as.VSSUB(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x8C860257);
}

TEST_CASE("VSSUB.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSUB(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8E85C257);

    as.RewindBuffer();

    as.VSSUB(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8C85C257);
}

TEST_CASE("VSSUBU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSUBU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x8A860257);

    as.RewindBuffer();

    as.VSSUBU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x88860257);
}

TEST_CASE("VSSUBU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSUBU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x8A85C257);

    as.RewindBuffer();

    as.VSSUBU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x8885C257);
}

TEST_CASE("VSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSUB(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x0A860257);

    as.RewindBuffer();

    as.VSUB(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x08860257);
}

TEST_CASE("VSUB.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSUB(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x0A85C257);

    as.RewindBuffer();

    as.VSUB(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x0885C257);
}

TEST_CASE("VWADD.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADD(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xC6862257);

    as.RewindBuffer();

    as.VWADD(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC4862257);
}

TEST_CASE("VWADD.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADD(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xC685E257);

    as.RewindBuffer();

    as.VWADD(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xC485E257);

    as.RewindBuffer();

    as.VWCVT(v4, v8, VecMask::No);
    REQUIRE(value == 0xC6806257);
}

TEST_CASE("VWADD.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADDW(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xD6862257);

    as.RewindBuffer();

    as.VWADDW(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xD4862257);
}

TEST_CASE("VWADD.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADDW(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xD685E257);

    as.RewindBuffer();

    as.VWADDW(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xD485E257);
}

TEST_CASE("VWADDU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADDU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xC2862257);

    as.RewindBuffer();

    as.VWADDU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC0862257);
}

TEST_CASE("VWADDU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADDU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xC285E257);

    as.RewindBuffer();

    as.VWADDU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xC085E257);

    as.RewindBuffer();

    as.VWCVTU(v4, v8, VecMask::No);
    REQUIRE(value == 0xC2806257);
}

TEST_CASE("VWADDU.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADDUW(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xD2862257);

    as.RewindBuffer();

    as.VWADDUW(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xD0862257);
}

TEST_CASE("VWADDU.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWADDUW(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xD285E257);

    as.RewindBuffer();

    as.VWADDUW(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xD085E257);
}

TEST_CASE("VWMACC.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACC(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xF6862257);

    as.RewindBuffer();

    as.VWMACC(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xF4862257);
}

TEST_CASE("VWMACC.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACC(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xF685E257);

    as.RewindBuffer();

    as.VWMACC(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xF485E257);
}

TEST_CASE("VWMACCSU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACCSU(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xFE862257);

    as.RewindBuffer();

    as.VWMACCSU(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xFC862257);
}

TEST_CASE("VWMACCSU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACCSU(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xFE85E257);

    as.RewindBuffer();

    as.VWMACCSU(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xFC85E257);
}

TEST_CASE("VWMACCU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACCU(v4, v12, v8, VecMask::No);
    REQUIRE(value == 0xF2862257);

    as.RewindBuffer();

    as.VWMACCU(v4, v12, v8, VecMask::Yes);
    REQUIRE(value == 0xF0862257);
}

TEST_CASE("VWMACCU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACCU(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xF285E257);

    as.RewindBuffer();

    as.VWMACCU(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xF085E257);
}

TEST_CASE("VWMACCUS.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMACCUS(v4, x11, v8, VecMask::No);
    REQUIRE(value == 0xFA85E257);

    as.RewindBuffer();

    as.VWMACCUS(v4, x11, v8, VecMask::Yes);
    REQUIRE(value == 0xF885E257);
}

TEST_CASE("VWMUL.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMUL(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xEE862257);

    as.RewindBuffer();

    as.VWMUL(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xEC862257);
}

TEST_CASE("VWMUL.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMUL(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xEE85E257);

    as.RewindBuffer();

    as.VWMUL(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xEC85E257);
}

TEST_CASE("VWMULSU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMULSU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xEA862257);

    as.RewindBuffer();

    as.VWMULSU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xE8862257);
}

TEST_CASE("VWMULSU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMULSU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xEA85E257);

    as.RewindBuffer();

    as.VWMULSU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xE885E257);
}

TEST_CASE("VWMULU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMULU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xE2862257);

    as.RewindBuffer();

    as.VWMULU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xE0862257);
}

TEST_CASE("VWMULU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWMULU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xE285E257);

    as.RewindBuffer();

    as.VWMULU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xE085E257);
}

TEST_CASE("VWREDSUM.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWREDSUM(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xC6860257);

    as.RewindBuffer();

    as.VWREDSUM(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC4860257);
}

TEST_CASE("VWREDSUMU.VS", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWREDSUMU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xC2860257);

    as.RewindBuffer();

    as.VWREDSUMU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC0860257);
}

TEST_CASE("VWSUB.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUB(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xCE862257);

    as.RewindBuffer();

    as.VWSUB(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xCC862257);
}

TEST_CASE("VWSUB.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUB(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xCE85E257);

    as.RewindBuffer();

    as.VWSUB(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xCC85E257);
}

TEST_CASE("VWSUB.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUBW(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xDE862257);

    as.RewindBuffer();

    as.VWSUBW(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xDC862257);
}

TEST_CASE("VWSUB.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUBW(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xDE85E257);

    as.RewindBuffer();

    as.VWSUBW(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xDC85E257);
}

TEST_CASE("VWSUBU.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUBU(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xCA862257);

    as.RewindBuffer();

    as.VWSUBU(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xC8862257);
}

TEST_CASE("VWSUBU.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUBU(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xCA85E257);

    as.RewindBuffer();

    as.VWSUBU(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xC885E257);
}

TEST_CASE("VWSUBU.WV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUBUW(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0xDA862257);

    as.RewindBuffer();

    as.VWSUBUW(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0xD8862257);
}

TEST_CASE("VWSUBU.WX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSUBUW(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0xDA85E257);

    as.RewindBuffer();

    as.VWSUBUW(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0xD885E257);
}

TEST_CASE("VXOR.VV", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VXOR(v4, v8, v12, VecMask::No);
    REQUIRE(value == 0x2E860257);

    as.RewindBuffer();

    as.VXOR(v4, v8, v12, VecMask::Yes);
    REQUIRE(value == 0x2C860257);
}

TEST_CASE("VXOR.VX", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VXOR(v4, v8, x11, VecMask::No);
    REQUIRE(value == 0x2E85C257);

    as.RewindBuffer();

    as.VXOR(v4, v8, x11, VecMask::Yes);
    REQUIRE(value == 0x2C85C257);
}

TEST_CASE("VXOR.VI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VXOR(v4, v8, 15, VecMask::No);
    REQUIRE(value == 0x2E87B257);

    as.RewindBuffer();

    as.VXOR(v4, v8, -16, VecMask::No);
    REQUIRE(value == 0x2E883257);

    as.RewindBuffer();

    as.VXOR(v4, v8, 15, VecMask::Yes);
    REQUIRE(value == 0x2C87B257);

    as.RewindBuffer();

    as.VXOR(v4, v8, -16, VecMask::Yes);
    REQUIRE(value == 0x2C883257);

    as.RewindBuffer();

    as.VNOT(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x2C8FB257);
}

TEST_CASE("VZEXT.VF2", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VZEXTVF2(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A832257);

    as.RewindBuffer();

    as.VZEXTVF2(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48832257);
}

TEST_CASE("VZEXT.VF4", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VZEXTVF4(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A822257);

    as.RewindBuffer();

    as.VZEXTVF4(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48822257);
}

TEST_CASE("VZEXT.VF8", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VZEXTVF8(v4, v8, VecMask::No);
    REQUIRE(value == 0x4A812257);

    as.RewindBuffer();

    as.VZEXTVF8(v4, v8, VecMask::Yes);
    REQUIRE(value == 0x48812257);
}

TEST_CASE("VLE8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE8(v0, x11, VecMask::No);
    REQUIRE(value == 0x02058007);

    as.RewindBuffer();

    as.VLE8(v0, x11, VecMask::Yes);
    REQUIRE(value == 0x00058007);
}

TEST_CASE("VLE16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE16(v0, x11, VecMask::No);
    REQUIRE(value == 0x0205D007);

    as.RewindBuffer();

    as.VLE16(v0, x11, VecMask::Yes);
    REQUIRE(value == 0x0005D007);
}

TEST_CASE("VLE32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE32(v0, x11, VecMask::No);
    REQUIRE(value == 0x0205E007);

    as.RewindBuffer();

    as.VLE32(v0, x11, VecMask::Yes);
    REQUIRE(value == 0x0005E007);
}

TEST_CASE("VLE64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE64(v0, x11, VecMask::No);
    REQUIRE(value == 0x0205F007);

    as.RewindBuffer();

    as.VLE64(v0, x11, VecMask::Yes);
    REQUIRE(value == 0x0005F007);
}

TEST_CASE("VLM.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLM(v0, x11);
    REQUIRE(value == 0x02B58007);
}

TEST_CASE("VLSE8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLSE8(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB50207);

    as.RewindBuffer();

    as.VLSE8(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B50207);
}

TEST_CASE("VLSE16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLSE16(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB55207);

    as.RewindBuffer();

    as.VLSE16(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B55207);
}

TEST_CASE("VLSE32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLSE32(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB56207);

    as.RewindBuffer();

    as.VLSE32(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B56207);
}

TEST_CASE("VLSE64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLSE64(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB57207);

    as.RewindBuffer();

    as.VLSE64(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B57207);
}

TEST_CASE("VLOXEI8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLOXEI8(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC50207);

    as.RewindBuffer();

    as.VLOXEI8(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC50207);
}

TEST_CASE("VLOXEI16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLOXEI16(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC55207);

    as.RewindBuffer();

    as.VLOXEI16(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC55207);
}

TEST_CASE("VLOXEI32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLOXEI32(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC56207);

    as.RewindBuffer();

    as.VLOXEI32(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC56207);
}

TEST_CASE("VLOXEI64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLOXEI64(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC57207);

    as.RewindBuffer();

    as.VLOXEI64(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC57207);
}

TEST_CASE("VLUXEI8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLUXEI8(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C50207);

    as.RewindBuffer();

    as.VLUXEI8(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C50207);
}

TEST_CASE("VLUXEI16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLUXEI16(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C55207);

    as.RewindBuffer();

    as.VLUXEI16(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C55207);
}

TEST_CASE("VLUXEI32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLUXEI32(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C56207);

    as.RewindBuffer();

    as.VLUXEI32(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C56207);
}

TEST_CASE("VLUXEI64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLUXEI64(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C57207);

    as.RewindBuffer();

    as.VLUXEI64(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C57207);
}

TEST_CASE("VLE8FF.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE8FF(v4, x10, VecMask::No);
    REQUIRE(value == 0x03050207);

    as.RewindBuffer();

    as.VLE8FF(v4, x10, VecMask::Yes);
    REQUIRE(value == 0x01050207);
}

TEST_CASE("VLE16FF.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE16FF(v4, x10, VecMask::No);
    REQUIRE(value == 0x03055207);

    as.RewindBuffer();

    as.VLE16FF(v4, x10, VecMask::Yes);
    REQUIRE(value == 0x01055207);
}

TEST_CASE("VLE32FF.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE32FF(v4, x10, VecMask::No);
    REQUIRE(value == 0x03056207);

    as.RewindBuffer();

    as.VLE32FF(v4, x10, VecMask::Yes);
    REQUIRE(value == 0x01056207);
}

TEST_CASE("VLE64FF.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLE64FF(v4, x10, VecMask::No);
    REQUIRE(value == 0x03057207);

    as.RewindBuffer();

    as.VLE64FF(v4, x10, VecMask::Yes);
    REQUIRE(value == 0x01057207);
}

TEST_CASE("8-bit segmented unit-stride loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSEGE8(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02050207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSEGE8(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00050207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit segmented unit-stride loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSEGE16(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02055207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSEGE16(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00055207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit segmented unit-stride loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSEGE32(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02056207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSEGE32(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00056207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit segmented unit-stride loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSEGE64(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02057207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSEGE64(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00057207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit segmented strided loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSSEGE8(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB50207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSSEGE8(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B50207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit segmented strided loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSSEGE16(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB55207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSSEGE16(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B55207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit segmented strided loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSSEGE32(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB56207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSSEGE32(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B56207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit segmented strided loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLSSEGE64(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB57207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLSSEGE64(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B57207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit vector indexed-ordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLOXSEGEI8(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC50207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLOXSEGEI8(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC50207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit vector indexed-ordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLOXSEGEI16(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC55207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLOXSEGEI16(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC55207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit vector indexed-ordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLOXSEGEI32(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC56207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLOXSEGEI32(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC56207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit vector indexed-ordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLOXSEGEI64(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC57207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLOXSEGEI64(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC57207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit vector indexed-unordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLUXSEGEI8(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C50207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLUXSEGEI8(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C50207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit vector indexed-unordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLUXSEGEI16(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C55207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLUXSEGEI16(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C55207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit vector indexed-unordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLUXSEGEI32(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C56207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLUXSEGEI32(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C56207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit vector indexed-unordered segment loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VLUXSEGEI64(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C57207 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VLUXSEGEI64(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C57207 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit vector whole register loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLRE8(1, v3, x10);
    REQUIRE(value == 0x02850187);
    as.RewindBuffer();

    as.VLRE8(2, v2, x10);
    REQUIRE(value == 0x22850107);
    as.RewindBuffer();

    as.VLRE8(4, v4, x10);
    REQUIRE(value == 0x62850207);
    as.RewindBuffer();

    as.VLRE8(8, v8, x10);
    REQUIRE(value == 0xE2850407);
}

TEST_CASE("16-bit vector whole register loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLRE16(1, v3, x10);
    REQUIRE(value == 0x02855187);
    as.RewindBuffer();

    as.VLRE16(2, v2, x10);
    REQUIRE(value == 0x22855107);
    as.RewindBuffer();

    as.VLRE16(4, v4, x10);
    REQUIRE(value == 0x62855207);
    as.RewindBuffer();

    as.VLRE16(8, v8, x10);
    REQUIRE(value == 0xE2855407);
}

TEST_CASE("32-bit vector whole register loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLRE32(1, v3, x10);
    REQUIRE(value == 0x02856187);
    as.RewindBuffer();

    as.VLRE32(2, v2, x10);
    REQUIRE(value == 0x22856107);
    as.RewindBuffer();

    as.VLRE32(4, v4, x10);
    REQUIRE(value == 0x62856207);
    as.RewindBuffer();

    as.VLRE32(8, v8, x10);
    REQUIRE(value == 0xE2856407);
}

TEST_CASE("64-bit vector whole register loads", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VLRE64(1, v3, x10);
    REQUIRE(value == 0x02857187);
    as.RewindBuffer();

    as.VLRE64(2, v2, x10);
    REQUIRE(value == 0x22857107);
    as.RewindBuffer();

    as.VLRE64(4, v4, x10);
    REQUIRE(value == 0x62857207);
    as.RewindBuffer();

    as.VLRE64(8, v8, x10);
    REQUIRE(value == 0xE2857407);
}

TEST_CASE("VSE8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSE8(v0, x13, VecMask::No);
    REQUIRE(value == 0x02068027);

    as.RewindBuffer();

    as.VSE8(v0, x13, VecMask::Yes);
    REQUIRE(value == 0x00068027);
}

TEST_CASE("VSE16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSE16(v0, x13, VecMask::No);
    REQUIRE(value == 0x0206D027);

    as.RewindBuffer();

    as.VSE16(v0, x13, VecMask::Yes);
    REQUIRE(value == 0x0006D027);
}

TEST_CASE("VSE32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSE32(v0, x13, VecMask::No);
    REQUIRE(value == 0x0206E027);

    as.RewindBuffer();

    as.VSE32(v0, x13, VecMask::Yes);
    REQUIRE(value == 0x0006E027);
}

TEST_CASE("VSE64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSE64(v0, x13, VecMask::No);
    REQUIRE(value == 0x0206F027);

    as.RewindBuffer();

    as.VSE64(v0, x13, VecMask::Yes);
    REQUIRE(value == 0x0006F027);
}

TEST_CASE("VSM.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSM(v0, x13);
    REQUIRE(value == 0x02B68027);
}

TEST_CASE("VSSE8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSE8(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB50227);

    as.RewindBuffer();

    as.VSSE8(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B50227);
}

TEST_CASE("VSSE16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSE16(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB55227);

    as.RewindBuffer();

    as.VSSE16(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B55227);
}

TEST_CASE("VSSE32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSE32(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB56227);

    as.RewindBuffer();

    as.VSSE32(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B56227);
}

TEST_CASE("VSSE64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSSE64(v4, x10, x11, VecMask::No);
    REQUIRE(value == 0x0AB57227);

    as.RewindBuffer();

    as.VSSE64(v4, x10, x11, VecMask::Yes);
    REQUIRE(value == 0x08B57227);
}

TEST_CASE("VSOXEI8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSOXEI8(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC50227);

    as.RewindBuffer();

    as.VSOXEI8(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC50227);
}

TEST_CASE("VSOXEI16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSOXEI16(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC55227);

    as.RewindBuffer();

    as.VSOXEI16(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC55227);
}

TEST_CASE("VSOXEI32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSOXEI32(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC56227);

    as.RewindBuffer();

    as.VSOXEI32(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC56227);
}

TEST_CASE("VSOXEI64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSOXEI64(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x0EC57227);

    as.RewindBuffer();

    as.VSOXEI64(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x0CC57227);
}

TEST_CASE("VSUXEI8.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSUXEI8(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C50227);

    as.RewindBuffer();

    as.VSUXEI8(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C50227);
}

TEST_CASE("VSUXEI16.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSUXEI16(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C55227);

    as.RewindBuffer();

    as.VSUXEI16(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C55227);
}

TEST_CASE("VSUXEI32.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSUXEI32(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C56227);

    as.RewindBuffer();

    as.VSUXEI32(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C56227);
}

TEST_CASE("VSUXEI64.V", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSUXEI64(v4, x10, v12, VecMask::No);
    REQUIRE(value == 0x06C57227);

    as.RewindBuffer();

    as.VSUXEI64(v4, x10, v12, VecMask::Yes);
    REQUIRE(value == 0x04C57227);
}

TEST_CASE("8-bit segmented unit-stride stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSEGE8(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02050227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSEGE8(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00050227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit segmented unit-stride stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSEGE16(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02055227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSEGE16(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00055227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit segmented unit-stride stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSEGE32(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02056227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSEGE32(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00056227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit segmented unit-stride stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSEGE64(i, v4, x10, VecMask::No);
        REQUIRE(value == (0x02057227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSEGE64(i, v4, x10, VecMask::Yes);
        REQUIRE(value == (0x00057227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit segmented strided stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSSEGE8(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB50227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSSEGE8(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B50227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit segmented strided stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSSEGE16(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB55227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSSEGE16(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B55227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit segmented strided stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSSEGE32(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB56227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSSEGE32(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B56227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit segmented strided stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSSSEGE64(i, v4, x10, x11, VecMask::No);
        REQUIRE(value == (0x0AB57227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSSSEGE64(i, v4, x10, x11, VecMask::Yes);
        REQUIRE(value == (0x08B57227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit segmented vector indexed-ordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSOXSEGEI8(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC50227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSOXSEGEI8(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC50227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit segmented vector indexed-ordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSOXSEGEI16(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC55227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSOXSEGEI16(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC55227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit segmented vector indexed-ordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSOXSEGEI32(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC56227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSOXSEGEI32(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC56227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit segmented vector indexed-ordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSOXSEGEI64(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x0EC57227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSOXSEGEI64(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x0CC57227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("8-bit segmented vector indexed-unordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSUXSEGEI8(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C50227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSUXSEGEI8(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C50227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("16-bit segmented vector indexed-unordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSUXSEGEI16(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C55227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSUXSEGEI16(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C55227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("32-bit segmented vector indexed-unordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSUXSEGEI32(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C56227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSUXSEGEI32(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C56227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("64-bit segmented vector indexed-unordered stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 2; i <= 8; i++) {
        as.VSUXSEGEI64(i, v4, x10, v12, VecMask::No);
        REQUIRE(value == (0x06C57227 | ((i - 1) << 29)));

        as.RewindBuffer();

        as.VSUXSEGEI64(i, v4, x10, v12, VecMask::Yes);
        REQUIRE(value == (0x04C57227 | ((i - 1) << 29)));

        as.RewindBuffer();
    }
}

TEST_CASE("Vector whole register stores", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSR(1, v3, x11);
    REQUIRE(value == 0x028581A7);

    as.RewindBuffer();

    as.VSR(2, v2, x11);
    REQUIRE(value == 0x22858127);

    as.RewindBuffer();

    as.VSR(4, v4, x11);
    REQUIRE(value == 0x62858227);

    as.RewindBuffer();

    as.VSR(8, v8, x11);
    REQUIRE(value == 0xE2858427);
}

TEST_CASE("VSETIVLI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSETIVLI(x10, 11, SEW::E8, LMUL::M1, VTA::No, VMA::No);
    REQUIRE(value == 0xC005F557);

    as.RewindBuffer();

    as.VSETIVLI(x10, 11, SEW::E16, LMUL::M2, VTA::No, VMA::No);
    REQUIRE(value == 0xC095F557);

    as.RewindBuffer();

    as.VSETIVLI(x10, 11, SEW::E256, LMUL::M2, VTA::Yes, VMA::No);
    REQUIRE(value == 0xC695F557);

    as.RewindBuffer();

    as.VSETIVLI(x10, 11, SEW::E256, LMUL::M2, VTA::Yes, VMA::Yes);
    REQUIRE(value == 0xCE95F557);
}

TEST_CASE("VSETVL", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSETVL(x10, x11, x12);
    REQUIRE(value == 0x80C5F557);
}

TEST_CASE("VSETVLI", "[rvv]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSETVLI(x31, x6, SEW::E64, LMUL::MF2, VTA::Yes, VMA::Yes);
    REQUIRE(value == 0x0DF37FD7);

    as.RewindBuffer();

    as.VSETVLI(x31, x6, SEW::E64, LMUL::MF2, VTA::No, VMA::No);
    REQUIRE(value == 0x01F37FD7);

    as.RewindBuffer();

    as.VSETVLI(x12, x18, SEW::E8, LMUL::M1, VTA::No, VMA::No);
    REQUIRE(value == 0x00097657);

    as.RewindBuffer();

    as.VSETVLI(x15, x12, SEW::E32, LMUL::M4, VTA::No, VMA::No);
    REQUIRE(value == 0x012677D7);
}
