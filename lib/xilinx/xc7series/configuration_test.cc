#include <prjxray/xilinx/xc7series/configuration.h>

#include <cstdint>
#include <iostream>
#include <vector>

#include <absl/types/span.h>
#include <gtest/gtest.h>
#include <prjxray/memory_mapped_file.h>
#include <prjxray/xilinx/xc7series/configuration_packet.h>
#include <prjxray/xilinx/xc7series/configuration_register.h>
#include <prjxray/xilinx/xc7series/frame_address.h>
#include <prjxray/xilinx/xc7series/frames.h>
#include <prjxray/xilinx/xc7series/part.h>
#include <prjxray/xilinx/xc7series/utils.h>
#include <yaml-cpp/yaml.h>

namespace xc7series = prjxray::xilinx::xc7series;
TEST(ConfigurationTest, ConstructFromPacketsWithSingleFrame) {
	std::vector<xc7series::FrameAddress> test_part_addresses;
	test_part_addresses.push_back(0x4567);
	test_part_addresses.push_back(0x4568);

	xc7series::Part test_part(0x1234, test_part_addresses);

	std::vector<uint32_t> idcode{0x1234};
	std::vector<uint32_t> cmd{0x0001};
	std::vector<uint32_t> frame_address{0x4567};
	std::vector<uint32_t> frame(101, 0xAA);

	std::vector<xc7series::ConfigurationPacket> packets{
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::IDCODE,
	        absl::MakeSpan(idcode),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::FAR,
	        absl::MakeSpan(frame_address),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::CMD,
	        absl::MakeSpan(cmd),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::FDRI,
	        absl::MakeSpan(frame),
	    },
	};

	auto test_config =
	    xc7series::Configuration::InitWithPackets(test_part, packets);
	ASSERT_TRUE(test_config);

	EXPECT_EQ(test_config->part().idcode(), static_cast<uint32_t>(0x1234));
	EXPECT_EQ(test_config->frames().size(), static_cast<size_t>(1));
	EXPECT_EQ(test_config->frames().at(0x4567), frame);
}

TEST(ConfigurationTest, ConstructFromPacketsWithAutoincrement) {
	std::vector<xc7series::FrameAddress> test_part_addresses;
	for (int ii = 0x4560; ii < 0x4570; ++ii) {
		test_part_addresses.push_back(ii);
	}
	for (int ii = 0x4580; ii < 0x4590; ++ii) {
		test_part_addresses.push_back(ii);
	}

	xc7series::Part test_part(0x1234, test_part_addresses);

	std::vector<uint32_t> idcode{0x1234};
	std::vector<uint32_t> cmd{0x0001};
	std::vector<uint32_t> frame_address{0x456F};
	std::vector<uint32_t> frame(202, 0xAA);
	std::fill_n(frame.begin() + 101, 101, 0xBB);

	std::vector<xc7series::ConfigurationPacket> packets{
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::IDCODE,
	        absl::MakeSpan(idcode),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::FAR,
	        absl::MakeSpan(frame_address),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::CMD,
	        absl::MakeSpan(cmd),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::FDRI,
	        absl::MakeSpan(frame),
	    },
	};

	auto test_config =
	    xc7series::Configuration::InitWithPackets(test_part, packets);
	ASSERT_TRUE(test_config);

	absl::Span<uint32_t> frame_span(frame);
	EXPECT_EQ(test_config->part().idcode(), static_cast<uint32_t>(0x1234));
	EXPECT_EQ(test_config->frames().size(), static_cast<size_t>(2));
	EXPECT_EQ(test_config->frames().at(0x456F),
	          std::vector<uint32_t>(101, 0xAA));
	EXPECT_EQ(test_config->frames().at(0x4580),
	          std::vector<uint32_t>(101, 0xBB));
}

TEST(ConfigurationTest,
     DebugAndPerFrameCrcBitstreamsProduceEqualConfigurations) {
	auto part = xc7series::Part::FromFile("configuration_test.yaml");
	ASSERT_TRUE(part);

	auto debug_bitstream = prjxray::MemoryMappedFile::InitWithFile(
	    "configuration_test.debug.bit");
	ASSERT_TRUE(debug_bitstream);

	auto debug_reader = xc7series::BitstreamReader::InitWithBytes(
	    debug_bitstream->as_bytes());
	ASSERT_TRUE(debug_reader);

	auto debug_configuration =
	    xc7series::Configuration::InitWithPackets(*part, *debug_reader);
	ASSERT_TRUE(debug_configuration);

	auto perframecrc_bitstream = prjxray::MemoryMappedFile::InitWithFile(
	    "configuration_test.perframecrc.bit");
	ASSERT_TRUE(perframecrc_bitstream);

	auto perframecrc_reader = xc7series::BitstreamReader::InitWithBytes(
	    perframecrc_bitstream->as_bytes());
	ASSERT_TRUE(perframecrc_reader);

	auto perframecrc_configuration =
	    xc7series::Configuration::InitWithPackets(*part,
	                                              *perframecrc_reader);
	ASSERT_TRUE(perframecrc_configuration);

	for (auto debug_frame : debug_configuration->frames()) {
		auto perframecrc_frame =
		    perframecrc_configuration->frames().find(debug_frame.first);
		if (perframecrc_frame ==
		    perframecrc_configuration->frames().end()) {
			ADD_FAILURE() << debug_frame.first
			              << ": missing in perframecrc bitstream";
			continue;
		}

		for (int ii = 0; ii < 101; ++ii) {
			EXPECT_EQ(perframecrc_frame->second[ii],
			          debug_frame.second[ii])
			    << debug_frame.first << ": word " << ii;
		}
	}

	for (auto perframecrc_frame : perframecrc_configuration->frames()) {
		auto debug_frame =
		    debug_configuration->frames().find(perframecrc_frame.first);
		if (debug_frame == debug_configuration->frames().end()) {
			ADD_FAILURE() << perframecrc_frame.first
			              << ": unexpectedly present in "
			                 "perframecrc bitstream";
		}
	}
}

TEST(ConfigurationTest, DebugAndNormalBitstreamsProduceEqualConfigurations) {
	auto part = xc7series::Part::FromFile("configuration_test.yaml");
	ASSERT_TRUE(part);

	auto debug_bitstream = prjxray::MemoryMappedFile::InitWithFile(
	    "configuration_test.debug.bit");
	ASSERT_TRUE(debug_bitstream);

	auto debug_reader = xc7series::BitstreamReader::InitWithBytes(
	    debug_bitstream->as_bytes());
	ASSERT_TRUE(debug_reader);

	auto debug_configuration =
	    xc7series::Configuration::InitWithPackets(*part, *debug_reader);
	ASSERT_TRUE(debug_configuration);

	auto normal_bitstream =
	    prjxray::MemoryMappedFile::InitWithFile("configuration_test.bit");
	ASSERT_TRUE(normal_bitstream);

	auto normal_reader = xc7series::BitstreamReader::InitWithBytes(
	    normal_bitstream->as_bytes());
	ASSERT_TRUE(normal_reader);

	auto normal_configuration =
	    xc7series::Configuration::InitWithPackets(*part, *normal_reader);
	ASSERT_TRUE(normal_configuration);

	for (auto debug_frame : debug_configuration->frames()) {
		auto normal_frame =
		    normal_configuration->frames().find(debug_frame.first);
		if (normal_frame == normal_configuration->frames().end()) {
			ADD_FAILURE() << debug_frame.first
			              << ": missing in normal bitstream";
			continue;
		}

		for (int ii = 0; ii < 101; ++ii) {
			EXPECT_EQ(normal_frame->second[ii],
			          debug_frame.second[ii])
			    << debug_frame.first << ": word " << ii;
		}
	}

	for (auto normal_frame : normal_configuration->frames()) {
		auto debug_frame =
		    debug_configuration->frames().find(normal_frame.first);
		if (debug_frame == debug_configuration->frames().end()) {
			ADD_FAILURE()
			    << normal_frame.first
			    << ": unexpectedly present in normal bitstream";
		}
	}
}

TEST(ConfigurationTest, CheckForPaddingFrames) {
	std::vector<xc7series::FrameAddress> test_part_addresses = {
	    xc7series::FrameAddress(xc7series::BlockType::CLB_IO_CLK, false, 0,
	                            0, 0),
	    xc7series::FrameAddress(xc7series::BlockType::CLB_IO_CLK, true, 0,
	                            0, 0),
	    xc7series::FrameAddress(xc7series::BlockType::CLB_IO_CLK, true, 1,
	                            0, 0),
	    xc7series::FrameAddress(xc7series::BlockType::BLOCK_RAM, false, 0,
	                            0, 0),
	    xc7series::FrameAddress(xc7series::BlockType::BLOCK_RAM, false, 1,
	                            0, 0)};

	auto test_part = absl::optional<xc7series::Part>(
	    xc7series::Part(0x1234, test_part_addresses));

	xc7series::Frames frames;
	frames.getFrames().emplace(std::make_pair(
	    test_part_addresses.at(0), std::vector<uint32_t>(101, 0xAA)));
	frames.getFrames().emplace(std::make_pair(
	    test_part_addresses.at(1), std::vector<uint32_t>(101, 0xBB)));
	frames.getFrames().emplace(std::make_pair(
	    test_part_addresses.at(2), std::vector<uint32_t>(101, 0xCC)));
	frames.getFrames().emplace(std::make_pair(
	    test_part_addresses.at(3), std::vector<uint32_t>(101, 0xDD)));
	frames.getFrames().emplace(std::make_pair(
	    test_part_addresses.at(4), std::vector<uint32_t>(101, 0xEE)));
	ASSERT_EQ(frames.getFrames().size(), 5);

	xc7series::PacketData packet_data =
	    xc7series::createType2ConfigurationPacketData(frames.getFrames(),
	                                                  test_part);
	// createType2ConfigurationPacketData should detect 4
	// row/half/block_type switches thus add 4*2 padding frames  moreover 2
	// extra padding frames are added at the end of the creation of the data
	// overall this gives us: 5(real frames) + 4*2 + 2 = 15 frames, which is
	// 15 * 101 = 1515 words
	EXPECT_EQ(packet_data.size(), 15 * 101);

	std::vector<uint32_t> idcode{0x1234};
	std::vector<uint32_t> cmd{0x0001};
	std::vector<uint32_t> frame_address{0x0};

	std::vector<xc7series::ConfigurationPacket> packets{
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::IDCODE,
	        absl::MakeSpan(idcode),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::FAR,
	        absl::MakeSpan(frame_address),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::CMD,
	        absl::MakeSpan(cmd),
	    },
	    {
	        static_cast<unsigned int>(0x1),
	        xc7series::ConfigurationPacket::Opcode::Write,
	        xc7series::ConfigurationRegister::FDRI,
	        absl::MakeSpan(packet_data),
	    },
	};

	auto test_config =
	    xc7series::Configuration::InitWithPackets(*test_part, packets);
	ASSERT_EQ(test_config->frames().size(), 5);
	for (auto& frame : test_config->frames()) {
		EXPECT_EQ(frame.second, frames.getFrames().at(frame.first));
	}
}
