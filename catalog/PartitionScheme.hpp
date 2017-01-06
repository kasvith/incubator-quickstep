/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 **/

#ifndef QUICKSTEP_CATALOG_PARTITION_SCHEME_HPP_
#define QUICKSTEP_CATALOG_PARTITION_SCHEME_HPP_

#include <cstddef>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "catalog/Catalog.pb.h"
#include "catalog/CatalogTypedefs.hpp"
#include "catalog/PartitionSchemeHeader.hpp"
#include "storage/StorageBlockInfo.hpp"
#include "threading/Mutex.hpp"
#include "threading/SharedMutex.hpp"
#include "threading/SpinSharedMutex.hpp"
#include "utility/Macros.hpp"

#include "glog/logging.h"

namespace quickstep {

class Type;

/** \addtogroup Catalog
 *  @{
 */

/**
 * @brief The class which stores the partitioning information and partitioned
 *        blocks for a particular relation.
 **/
class PartitionScheme {
 public:
  /**
   * @brief Constructor.
   * @note The constructor takes ownership of \c header.
   *
   * @param header The partition header.
   * @param blocks_in_partition All blocks in partitions.
   **/
  explicit PartitionScheme(PartitionSchemeHeader *header)
      : header_(DCHECK_NOTNULL(header)),
        blocks_in_partition_(header_->getNumPartitions()),
        blocks_in_partition_mutexes_(header_->getNumPartitions()) {}

  /**
   * @brief Constructor.
   * @note The constructor takes ownership of \c header.
   *
   * @param header The partition header.
   * @param blocks_in_partition All blocks in partitions.
   **/
  PartitionScheme(PartitionSchemeHeader *header,
                  std::vector<std::unordered_set<block_id>> &&blocks_in_partition)
      : header_(DCHECK_NOTNULL(header)),
        blocks_in_partition_(std::move(blocks_in_partition)),
        blocks_in_partition_mutexes_(header_->getNumPartitions()) {}

  /**
   * @brief Reconstruct a Partition Scheme from its serialized
   *        Protocol Buffer form.
   *
   * @param proto The Protocol Buffer serialization of a Partition Scheme,
   *        previously produced by getProto().
   * @param attr_type The attribute type of the partitioning attribute.
   * @return The deserialized partition scheme object.
   **/
  static PartitionScheme* ReconstructFromProto(const serialization::PartitionScheme &proto,
                                               const Type &attr_type);

  /**
   * @brief Check whether a serialization::PartitionScheme is fully-formed and
   *        all parts are valid.
   *
   * @param proto A serialized Protocol Buffer representation of a
   *              PartitionScheme, originally generated by getProto().
   * @return Whether proto is fully-formed and valid.
   **/
  static bool ProtoIsValid(const serialization::PartitionScheme &proto);

  /**
   * @brief Get the partitioning attribute for the relation.
   *
   * @return The partitioning attribute with which the relation
   *         is partitioned into.
   **/
  const PartitionSchemeHeader& getPartitionSchemeHeader() const {
    return *header_;
  }

  /**
   * @brief Add a block to a partition.
   *
   * @param block_id The id of the block to be added to the partition.
   * @param part_id The id of the partition to add the block to.
   **/
  inline void addBlockToPartition(const block_id block,
                                  const partition_id part_id) {
    DCHECK_LT(part_id, header_->getNumPartitions());
    SpinSharedMutexExclusiveLock<false> lock(
        blocks_in_partition_mutexes_[part_id]);
    blocks_in_partition_[part_id].insert(block);
  }

  /**
   * @brief Remove a block from a partition.
   *
   * @param block_id The id of the block to be removed from the partition.
   * @param part_id The id of the partition to remove the block from.
   **/
  inline void removeBlockFromPartition(const block_id block,
                                       const partition_id part_id) {
    DCHECK_LT(part_id, header_->getNumPartitions());
    SpinSharedMutexExclusiveLock<false> lock(
        blocks_in_partition_mutexes_[part_id]);
    std::unordered_set<block_id> &blocks_in_partition =
        blocks_in_partition_[part_id];
    blocks_in_partition.erase(block);
  }

  /**
   * @brief Get all the blocks from a particular partition.
   *
   * @param part_id The id of the partition to retrieve the blocks from.
   * @return The block_ids of blocks belonging to this partition at the moment
   *         when this method is called.
   **/
  inline std::vector<block_id> getBlocksInPartition(
      const partition_id part_id) const {
    DCHECK_LT(part_id, header_->getNumPartitions());
    SpinSharedMutexSharedLock<false> lock(
        blocks_in_partition_mutexes_[part_id]);
    return std::vector<block_id>(blocks_in_partition_[part_id].begin(),
                                 blocks_in_partition_[part_id].end());
  }

  /**
   * @brief Serialize the Partition Scheme as Protocol Buffer.
   *
   * @return The Protocol Buffer representation of Partition Scheme.
   **/
  serialization::PartitionScheme getProto() const;

  /**
   * @brief Get the partition id for a block.
   *
   * @param block The id of the block.
   *
   * @return The partition id to which the block belongs. If the block is not
   *         found in any partition, then the maximum finite value for the type
   *         std::size_t is returned.
   **/
  partition_id getPartitionForBlock(const block_id block) const;

 private:
  std::unique_ptr<const PartitionSchemeHeader> header_;

  // The unordered set of blocks per partition.
  std::vector<std::unordered_set<block_id>> blocks_in_partition_;
  // Mutexes for locking each partition separately.
  mutable std::vector<SpinSharedMutex<false>> blocks_in_partition_mutexes_;

  DISALLOW_COPY_AND_ASSIGN(PartitionScheme);
};

/** @} */

}  // namespace quickstep

#endif  // QUICKSTEP_CATALOG_PARTITION_SCHEME_HPP_