#include "state_history.h"

#include <zlib.h>

#include <future>
#include <iostream>

Snapshot::SectionInfo Snapshot::get_component_info() const {
  if (buffer.size() < 24)
    return {0, 0, 24};
  uint32_t size, uncomp;
  std::memcpy(&size, buffer.data(), 4);
  std::memcpy(&uncomp, buffer.data() + 4, 4);
  return {size, uncomp, 24};
}

Snapshot::SectionInfo Snapshot::get_entity_info() const {
  if (buffer.size() < 24)
    return {0, 0, 24};
  auto comp_info = get_component_info();
  uint32_t size, uncomp;
  std::memcpy(&size, buffer.data() + 8, 4);
  std::memcpy(&uncomp, buffer.data() + 12, 4);
  return {size, uncomp, comp_info.offset + comp_info.size};
}

Snapshot::SectionInfo Snapshot::get_relationship_info() const {
  if (buffer.size() < 24)
    return {0, 0, 24};
  auto entity_info = get_entity_info();
  uint32_t size, uncomp;
  std::memcpy(&size, buffer.data() + 16, 4);
  std::memcpy(&uncomp, buffer.data() + 20, 4);
  return {size, uncomp, entity_info.offset + entity_info.size};
}

std::vector<uint8_t> Snapshot::get_decompressed_buffer() const {
  auto info = get_component_info();
  if (info.size == 0)
    return {};

  if (!is_component_compressed()) {
    // Extract component section
    if (buffer.size() < info.offset + info.size)
      return {};
    return std::vector<uint8_t>(buffer.begin() + info.offset, buffer.begin() + info.offset + info.size);
  }

  if (info.uncompressed_size == 0)
    return {};

  std::vector<uint8_t> decompressed(info.uncompressed_size);
  uLongf dest_len = info.uncompressed_size;

  int result = uncompress(decompressed.data(), &dest_len, buffer.data() + info.offset, info.size);

  if (result != Z_OK) {
    std::cerr << "Decompression failed with error: " << result << "\n";
    return {};
  }

  return decompressed;
}

size_t Snapshot::header_count() const {
  auto decompressed = get_decompressed_buffer();
  if (decompressed.size() < sizeof(uint32_t))
    return 0;
  uint32_t count;
  std::memcpy(&count, decompressed.data(), sizeof(uint32_t));
  return count;
}

const ComponentHeader* Snapshot::get_header(size_t index, const std::vector<uint8_t>& decompressed_buffer) const {
  size_t offset = sizeof(uint32_t) + index * sizeof(ComponentHeader);
  if (offset + sizeof(ComponentHeader) > decompressed_buffer.size()) {
    return nullptr;
  }
  return reinterpret_cast<const ComponentHeader*>(decompressed_buffer.data() + offset);
}

const uint8_t* Snapshot::get_data(const ComponentHeader* header,
                                  const std::vector<uint8_t>& decompressed_buffer) const {
  if (!header)
    return nullptr;

  uint32_t count;
  std::memcpy(&count, decompressed_buffer.data(), sizeof(uint32_t));
  size_t data_section_start = sizeof(uint32_t) + count * sizeof(ComponentHeader);
  size_t data_offset = data_section_start + header->offset;

  if (data_offset + header->size > decompressed_buffer.size()) {
    return nullptr;
  }

  return decompressed_buffer.data() + data_offset;
}

std::vector<uint8_t> Snapshot::get_decompressed_relationships() const {
  auto info = get_relationship_info();
  if (info.size == 0)
    return {};

  if (!is_relationship_compressed()) {
    // Extract relationship section
    if (buffer.size() < info.offset + info.size)
      return {};
    return std::vector<uint8_t>(buffer.begin() + info.offset, buffer.begin() + info.offset + info.size);
  }

  if (info.uncompressed_size == 0)
    return {};

  std::vector<uint8_t> decompressed(info.uncompressed_size);
  uLongf dest_len = info.uncompressed_size;

  int result = uncompress(decompressed.data(), &dest_len, buffer.data() + info.offset, info.size);

  if (result != Z_OK) {
    std::cerr << "Relationship decompression failed with error: " << result << "\n";
    return {};
  }

  return decompressed;
}

std::vector<RelationshipHeader> Snapshot::decode_relationships() const {
  std::vector<RelationshipHeader> relationships;
  auto buffer = get_decompressed_relationships();

  if (buffer.size() < sizeof(uint32_t))
    return relationships;

  uint32_t count;
  std::memcpy(&count, buffer.data(), sizeof(uint32_t));

  size_t offset = sizeof(uint32_t);
  flecs::entity_t prev_entity = 0;

  for (uint32_t i = 0; i < count && offset + sizeof(RelationshipHeader) <= buffer.size(); ++i) {
    RelationshipHeader header = {};
    std::memcpy(&header, buffer.data() + offset, sizeof(RelationshipHeader));

    // Decode delta-encoded entity ID
    header.entity += prev_entity;
    prev_entity = header.entity;

    relationships.push_back(header);
    offset += sizeof(RelationshipHeader);
  }

  return relationships;
}

std::vector<uint8_t> Snapshot::get_decompressed_entities() const {
  auto info = get_entity_info();
  if (info.size == 0)
    return {};

  if (!is_entity_compressed()) {
    // Extract entity section
    if (buffer.size() < info.offset + info.size)
      return {};
    return std::vector<uint8_t>(buffer.begin() + info.offset, buffer.begin() + info.offset + info.size);
  }

  if (info.uncompressed_size == 0)
    return {};

  std::vector<uint8_t> decompressed(info.uncompressed_size);
  uLongf dest_len = info.uncompressed_size;

  int result = uncompress(decompressed.data(), &dest_len, buffer.data() + info.offset, info.size);

  if (result != Z_OK) {
    std::cerr << "Entity decompression failed with error: " << result << "\n";
    return {};
  }

  return decompressed;
}

Snapshot::EntityLifecycle Snapshot::decode_entities() const {
  EntityLifecycle lifecycle;
  auto buffer = get_decompressed_entities();

  if (buffer.size() < sizeof(uint32_t) * 3)
    return lifecycle;

  size_t offset = 0;

  // Decode entities_created
  uint32_t created_count;
  std::memcpy(&created_count, buffer.data() + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < created_count && offset < buffer.size(); ++i) {
    flecs::entity_t id;
    std::memcpy(&id, buffer.data() + offset, sizeof(flecs::entity_t));
    offset += sizeof(flecs::entity_t);

    uint16_t name_len;
    std::memcpy(&name_len, buffer.data() + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    std::string name;
    if (name_len > 0 && offset + name_len <= buffer.size()) {
      name.assign(reinterpret_cast<const char*>(buffer.data() + offset), name_len);
      offset += name_len;
    }

    lifecycle.entities_created.push_back({id, name});
  }

  // Decode entities_destroyed
  uint32_t destroyed_count;
  std::memcpy(&destroyed_count, buffer.data() + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < destroyed_count && offset + sizeof(flecs::entity_t) <= buffer.size(); ++i) {
    flecs::entity_t id;
    std::memcpy(&id, buffer.data() + offset, sizeof(flecs::entity_t));
    offset += sizeof(flecs::entity_t);
    lifecycle.entities_destroyed.push_back(id);
  }

  // Decode existing_entities (for keyframes)
  uint32_t existing_count;
  std::memcpy(&existing_count, buffer.data() + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < existing_count && offset + sizeof(flecs::entity_t) <= buffer.size(); ++i) {
    flecs::entity_t id;
    std::memcpy(&id, buffer.data() + offset, sizeof(flecs::entity_t));
    offset += sizeof(flecs::entity_t);
    lifecycle.existing_entities.insert(id);
  }

  return lifecycle;
}
void StateHistory::setup_observers() {
  // Track operations for all registered components using C API
  for (ecs_entity_t comp_id : tracked_component_ids) {
    // OnAdd observer
    ecs_observer_desc_t add_desc = {};
    add_desc.query.terms[0].id = comp_id;
    add_desc.events[0] = EcsOnAdd;
    add_desc.callback = [](ecs_iter_t* it) {
      StateHistory* self = static_cast<StateHistory*>(it->ctx);
      ecs_entity_t comp_id = ecs_field_id(it, 0);
      for (int i = 0; i < it->count; i++) {
        self->record_event(it->entities[i], comp_id, ComponentOp::Add);
      }
    };
    add_desc.ctx = this;
    add_observers.push_back(ecs_observer_init(world->c_ptr(), &add_desc));

    // OnRemove observer
    ecs_observer_desc_t remove_desc = {};
    remove_desc.query.terms[0].id = comp_id;
    remove_desc.events[0] = EcsOnRemove;
    remove_desc.callback = [](ecs_iter_t* it) {
      StateHistory* self = static_cast<StateHistory*>(it->ctx);
      ecs_entity_t comp_id = ecs_field_id(it, 0);
      for (int i = 0; i < it->count; i++) {
        self->record_event(it->entities[i], comp_id, ComponentOp::Remove);
      }
    };
    remove_desc.ctx = this;
    remove_observers.push_back(ecs_observer_init(world->c_ptr(), &remove_desc));

    // OnSet observer
    ecs_observer_desc_t set_desc = {};
    set_desc.query.terms[0].id = comp_id;
    set_desc.events[0] = EcsOnSet;
    set_desc.callback = [](ecs_iter_t* it) {
      StateHistory* self = static_cast<StateHistory*>(it->ctx);
      ecs_entity_t comp_id = ecs_field_id(it, 0);
      for (int i = 0; i < it->count; i++) {
        self->record_event(it->entities[i], comp_id, ComponentOp::Set);
      }
    };
    set_desc.ctx = this;
    set_observers.push_back(ecs_observer_init(world->c_ptr(), &set_desc));
  }
  // Track all tag/component additions using wildcard (single wildcard for
  // non-pair components)
  wildcard_add_observer = world->observer().with(flecs::Wildcard).event(flecs::OnAdd).each([this](flecs::iter& it, size_t index) {
    auto e = it.entity(index);
    auto id = it.id(0);

    // Skip if this is a pair (handled by relationship observer)
    if (ECS_IS_PAIR(id)) {
      return;
    }

    auto component_id = id;

    // Skip if this is a component we're already tracking explicitly
    if (std::find(tracked_component_ids.begin(), tracked_component_ids.end(), component_id) !=
        tracked_component_ids.end()) {
      return;
    }

    // Filter out flecs internal components
    auto flecs_module = world->lookup("flecs");
    auto identifier = world->lookup("Identifier");

    if (component_id == flecs_module.id() || component_id == identifier.id() || component_id == EcsObserver ||
        component_id == EcsQuery || component_id == EcsSystem || component_id == flecs::ChildOf ||
        component_id == EcsDependsOn) {
      return;
    }

    // Record as a tag (component with size 0)
    record_event(e.id(), component_id, ComponentOp::Add);
  }).id();

  // Track all tag/component removals using wildcard
  wildcard_remove_observer = world->observer().with(flecs::Wildcard).event(flecs::OnRemove).each([this](flecs::iter& it, size_t index) {
    auto e = it.entity(index);
    auto id = it.id(0);

    // Skip if this is a pair (handled by relationship observer)
    if (ECS_IS_PAIR(id)) {
      return;
    }

    auto component_id = id;

    // Skip if this is a component we're already tracking explicitly
    if (std::find(tracked_component_ids.begin(), tracked_component_ids.end(), component_id) !=
        tracked_component_ids.end()) {
      return;
    }

    // Filter out flecs internal components
    auto flecs_module = world->lookup("flecs");
    auto identifier = world->lookup("Identifier");

    if (component_id == flecs_module.id() || component_id == identifier.id() || component_id == EcsObserver ||
        component_id == EcsQuery || component_id == EcsSystem || component_id == flecs::ChildOf ||
        component_id == EcsDependsOn) {
      return;
    }

    // Record as a tag removal
    record_event(e.id(), component_id, ComponentOp::Remove);
  }).id();

  // Track all relationship additions using wildcards
  wildcard_rel_add_observer = world->observer()
      .with(flecs::Wildcard, flecs::Wildcard)
      .event(flecs::OnAdd)
      .each([this](flecs::iter& it, size_t index) {
        auto e = it.entity(index);
        auto pair = it.pair(0);
        auto rel = pair.first();
        auto tgt = pair.second();
        record_relationship(e.id(), rel, tgt, ComponentOp::Add);
      }).id();

  // Track all relationship removals using wildcards
  wildcard_rel_remove_observer = world->observer()
      .with(flecs::Wildcard, flecs::Wildcard)
      .event(flecs::OnRemove)
      .each([this](flecs::iter& it, size_t index) {
        auto e = it.entity(index);
        auto pair = it.pair(0);
        auto rel = pair.first();
        auto tgt = pair.second();
        record_relationship(e.id(), rel, tgt, ComponentOp::Remove);
      }).id();
}
void StateHistory::record_event(flecs::entity_t entity, flecs::entity_t component, ComponentOp op) {
  if (recording_enabled) {
    frame_events.push_back({entity, component, op});
  }
}
void StateHistory::record_relationship(flecs::entity_t entity,
                                       flecs::entity_t relation,
                                       flecs::entity_t target,
                                       ComponentOp op) {
  if (recording_enabled) {
    // Only track relationships for entities we're explicitly tracking
    if (tracked_entities.find(entity) == tracked_entities.end()) {
      return;
    }

    // Filter out flecs internal relationships
    // Skip ChildOf relationships
    if (relation == flecs::ChildOf) {
      return;
    }

    // Get flecs module and other internal entities
    auto flecs_module = world->lookup("flecs");
    auto identifier = world->lookup("Identifier");

    // Skip if any of these flecs-native conditions are true:
    // - Source is EcsFlecs
    // - Relation is Identifier
    // - Target is EcsObserver, EcsQuery, or EcsSystem
    // - Relation is EcsDependsOn
    if (entity == flecs_module.id() || relation == identifier.id() || target == EcsObserver || target == EcsQuery ||
        target == EcsSystem || relation == EcsDependsOn) {
      return;
    }

    relationship_events.push_back({entity, relation, target, op});
  }
}
void StateHistory::track_entity(flecs::entity e) {
  flecs::entity_t id = e.id();
  if (tracked_entities.find(id) == tracked_entities.end()) {
    tracked_entities.insert(id);

    // Store entity name if it has one
    const char* name = e.name();
    if (name) {
      entity_names[id] = std::string(name);
      if (recording_enabled) {
        entity_events.push_back({id, EntityOp::Create, std::string(name)});
      }
    } else {
      if (recording_enabled) {
        entity_events.push_back({id, EntityOp::Create, ""});
      }
    }

    // Capture any existing relationships that were added before tracking
    // This fixes the bug where relationships added before track_entity() aren't
    // recorded
    if (recording_enabled) {
      auto flecs_module = world->lookup("flecs");
      auto identifier = world->lookup("Identifier");

      e.each([&](flecs::id rel_id) {
        if (rel_id.is_pair()) {
          auto rel = rel_id.first();
          auto tgt = rel_id.second();

          // Apply same filters as record_relationship
          if (rel == flecs::ChildOf) {
            return;
          }

          if (id == flecs_module.id() || rel == identifier.id() || tgt == EcsObserver || tgt == EcsQuery ||
              tgt == EcsSystem || rel == EcsDependsOn) {
            return;
          }

          // Record the existing relationship
          relationship_events.push_back({id, rel, tgt, ComponentOp::Add});
        }
      });
    }
  }
}
void StateHistory::untrack_entity(flecs::entity e) {
  flecs::entity_t id = e.id();
  if (tracked_entities.find(id) != tracked_entities.end()) {
    std::string name = entity_names.count(id) ? entity_names[id] : "";
    if (recording_enabled) {
      entity_events.push_back({id, EntityOp::Destroy, name});
    }
    tracked_entities.erase(id);
    entity_names.erase(id);
  }
}
std::vector<uint8_t> StateHistory::compress_buffer(const std::vector<uint8_t>& input) {
  if (input.empty())
    return input;

  uLongf compressed_size = compressBound(input.size());
  std::vector<uint8_t> compressed(compressed_size);

  int result = compress2(compressed.data(), &compressed_size, input.data(), input.size(), Z_BEST_SPEED);

  if (result != Z_OK) {
    return input;
  }

  compressed.resize(compressed_size);
  compressed.shrink_to_fit();
  return compressed;
}
void StateHistory::finalize_snapshot(Snapshot& snapshot,
                                     const SectionData& component_section,
                                     const SectionData& entity_section,
                                     const SectionData& relationship_section) {
  // Build header: 6 x uint32_t
  std::vector<uint8_t> combined;
  combined.reserve(24 + component_section.data.size() + entity_section.data.size() + relationship_section.data.size());

  // Write header
  uint32_t comp_size = static_cast<uint32_t>(component_section.data.size());
  uint32_t comp_uncomp = component_section.uncompressed_size;
  uint32_t ent_size = static_cast<uint32_t>(entity_section.data.size());
  uint32_t ent_uncomp = entity_section.uncompressed_size;
  uint32_t rel_size = static_cast<uint32_t>(relationship_section.data.size());
  uint32_t rel_uncomp = relationship_section.uncompressed_size;

  combined.insert(combined.end(), reinterpret_cast<uint8_t*>(&comp_size), reinterpret_cast<uint8_t*>(&comp_size) + 4);
  combined.insert(combined.end(), reinterpret_cast<uint8_t*>(&comp_uncomp),
                  reinterpret_cast<uint8_t*>(&comp_uncomp) + 4);
  combined.insert(combined.end(), reinterpret_cast<uint8_t*>(&ent_size), reinterpret_cast<uint8_t*>(&ent_size) + 4);
  combined.insert(combined.end(), reinterpret_cast<uint8_t*>(&ent_uncomp), reinterpret_cast<uint8_t*>(&ent_uncomp) + 4);
  combined.insert(combined.end(), reinterpret_cast<uint8_t*>(&rel_size), reinterpret_cast<uint8_t*>(&rel_size) + 4);
  combined.insert(combined.end(), reinterpret_cast<uint8_t*>(&rel_uncomp), reinterpret_cast<uint8_t*>(&rel_uncomp) + 4);

  // Append data sections
  combined.insert(combined.end(), component_section.data.begin(), component_section.data.end());
  combined.insert(combined.end(), entity_section.data.begin(), entity_section.data.end());
  combined.insert(combined.end(), relationship_section.data.begin(), relationship_section.data.end());

  snapshot.buffer = std::move(combined);
  snapshot.buffer.shrink_to_fit();
  snapshot.total_uncompressed_size = comp_uncomp + ent_uncomp + rel_uncomp;

  // Set flags
  snapshot.set_component_compressed(component_section.compressed);
  snapshot.set_entity_compressed(entity_section.compressed);
  snapshot.set_relationship_compressed(relationship_section.compressed);
}
StateHistory::SectionData StateHistory::encode_and_compress_entities(
    const std::vector<std::pair<flecs::entity_t, std::string>>& entities_created,
    const std::vector<flecs::entity_t>& entities_destroyed,
    const std::unordered_set<flecs::entity_t>& existing_entities) {
  // Build uncompressed buffer
  std::vector<uint8_t> uncompressed;

  // Encode entities_created
  uint32_t created_count = static_cast<uint32_t>(entities_created.size());
  uncompressed.insert(uncompressed.end(), reinterpret_cast<uint8_t*>(&created_count),
                      reinterpret_cast<uint8_t*>(&created_count) + sizeof(uint32_t));

  for (const auto& [id, name] : entities_created) {
    uncompressed.insert(uncompressed.end(), reinterpret_cast<const uint8_t*>(&id),
                        reinterpret_cast<const uint8_t*>(&id) + sizeof(flecs::entity_t));

    uint16_t name_len = static_cast<uint16_t>(name.size());
    uncompressed.insert(uncompressed.end(), reinterpret_cast<uint8_t*>(&name_len),
                        reinterpret_cast<uint8_t*>(&name_len) + sizeof(uint16_t));

    if (name_len > 0) {
      uncompressed.insert(uncompressed.end(), name.begin(), name.end());
    }
  }

  // Encode entities_destroyed
  uint32_t destroyed_count = static_cast<uint32_t>(entities_destroyed.size());
  uncompressed.insert(uncompressed.end(), reinterpret_cast<uint8_t*>(&destroyed_count),
                      reinterpret_cast<uint8_t*>(&destroyed_count) + sizeof(uint32_t));

  for (flecs::entity_t id : entities_destroyed) {
    uncompressed.insert(uncompressed.end(), reinterpret_cast<const uint8_t*>(&id),
                        reinterpret_cast<const uint8_t*>(&id) + sizeof(flecs::entity_t));
  }

  // Encode existing_entities
  uint32_t existing_count = static_cast<uint32_t>(existing_entities.size());
  uncompressed.insert(uncompressed.end(), reinterpret_cast<uint8_t*>(&existing_count),
                      reinterpret_cast<uint8_t*>(&existing_count) + sizeof(uint32_t));

  for (flecs::entity_t id : existing_entities) {
    uncompressed.insert(uncompressed.end(), reinterpret_cast<const uint8_t*>(&id),
                        reinterpret_cast<const uint8_t*>(&id) + sizeof(flecs::entity_t));
  }

  SectionData section;
  section.uncompressed_size = static_cast<uint32_t>(uncompressed.size());

  // Compress if enabled and not empty
  if (!uncompressed.empty()) {
    if (enable_compression) {
      section.data = compress_buffer(uncompressed);
      section.compressed = true;
    } else {
      section.data = std::move(uncompressed);
      section.compressed = false;
    }
    section.data.shrink_to_fit();
  } else {
    section.compressed = false;
  }

  return section;
}
void StateHistory::capture_state() {
  Snapshot snapshot;
  snapshot.frame = current_frame;
  snapshot.set_keyframe(current_frame % keyframe_interval == 0);

  std::vector<ComponentHeader> headers;
  std::vector<uint8_t> data_section;

  // Capture entity lifecycle events
  std::vector<std::pair<flecs::entity_t, std::string>> entities_created_vec;
  std::vector<flecs::entity_t> entities_destroyed_vec;
  std::unordered_set<flecs::entity_t> existing_entities_set;

  if (snapshot.is_keyframe()) {
    // For keyframes, store complete list of existing entities
    existing_entities_set = tracked_entities;
    // Also store entity names for recreation
    for (flecs::entity_t id : tracked_entities) {
      if (entity_names.count(id)) {
        entities_created_vec.push_back({id, entity_names[id]});
      }
    }
  } else {
    // For diff frames, store entity creation/destruction events
    for (const auto& event : entity_events) {
      if (event.op == EntityOp::Create) {
        entities_created_vec.push_back({event.entity, event.name});
      } else if (event.op == EntityOp::Destroy) {
        entities_destroyed_vec.push_back(event.entity);
      }
    }
  }

  // Launch entity encoding/compression async
  auto entity_future =
      std::async(std::launch::async, [this, entities_created_vec, entities_destroyed_vec, existing_entities_set]() {
        return encode_and_compress_entities(entities_created_vec, entities_destroyed_vec, existing_entities_set);
      });

  // Capture components (must be sequential - queries ECS world)
  if (snapshot.is_keyframe()) {
    // For keyframes, capture ALL components
    capture_all_components(snapshot, headers, data_section);
  } else {
    // For diff frames, only capture changed components
    capture_changed_components(snapshot, headers, data_section);
  }

  // Pack component buffer and launch compression + relationship capture in
  // parallel
  SectionData component_section;
  std::future<SectionData> component_future;
  std::future<SectionData> relationship_future;

  if (!headers.empty()) {
    size_t total_size = sizeof(uint32_t) + headers.size() * sizeof(ComponentHeader) + data_section.size();

    std::vector<uint8_t> uncompressed_buffer;
    uncompressed_buffer.reserve(total_size);

    uint32_t count = static_cast<uint32_t>(headers.size());
    uncompressed_buffer.insert(uncompressed_buffer.end(), reinterpret_cast<uint8_t*>(&count),
                               reinterpret_cast<uint8_t*>(&count) + sizeof(uint32_t));

    uncompressed_buffer.insert(uncompressed_buffer.end(), reinterpret_cast<uint8_t*>(headers.data()),
                               reinterpret_cast<uint8_t*>(headers.data() + headers.size()));

    uncompressed_buffer.insert(uncompressed_buffer.end(), data_section.begin(), data_section.end());

    uint32_t uncompressed_size = static_cast<uint32_t>(uncompressed_buffer.size());

    // Launch component compression async
    component_future = std::async(
        std::launch::async, [this, uncompressed_buffer = std::move(uncompressed_buffer), uncompressed_size]() mutable {
          SectionData section;
          section.uncompressed_size = uncompressed_size;

          if (enable_compression) {
            section.data = compress_buffer(uncompressed_buffer);
            section.compressed = true;
          } else {
            section.data = std::move(uncompressed_buffer);
            section.compressed = false;
          }

          section.data.shrink_to_fit();
          return section;
        });
  } else {
    // Empty component section
    component_future = std::async(std::launch::async, []() {
      SectionData section;
      section.uncompressed_size = 0;
      section.compressed = false;
      return section;
    });
  }

  // Launch relationship capture async
  if (snapshot.is_keyframe()) {
    relationship_future = std::async(std::launch::async, [this]() { return capture_all_relationships(); });
  } else {
    relationship_future = std::async(std::launch::async, [this]() { return capture_changed_relationships(); });
  }

  // Wait for all three async operations to complete
  SectionData entity_section = entity_future.get();
  component_section = component_future.get();
  SectionData relationship_section = relationship_future.get();

  // Finalize snapshot with all sections
  finalize_snapshot(snapshot, component_section, entity_section, relationship_section);

  // If we're capturing in the middle of the timeline, erase future frames
  // (branch behavior)
  if (current_frame < snapshots.size()) {
    snapshots.erase(snapshots.begin() + current_frame, snapshots.end());
  }

  snapshots.push_back(std::move(snapshot));
  current_frame++;

  // Clear events for next frame
  frame_events.clear();
  entity_events.clear();
  relationship_events.clear();

  if (snapshots.size() <= 10 || snapshots.size() % 50 == 0) {
    const auto& last = snapshots.back();
    std::cout << "  [Captured frame " << (snapshots.size() - 1);
    if (last.is_keyframe())
      std::cout << " - KEYFRAME";
    if (last.buffer.size() > 24 && last.total_uncompressed_size > 0) {
      double ratio = (double)last.total_uncompressed_size / (last.buffer.size() - 24);
      std::cout << " - compressed " << last.total_uncompressed_size << " -> " << (last.buffer.size() - 24) << " bytes ("
                << ratio << "x)";
    }
    std::cout << "]\n";
  }
}
void StateHistory::capture_all_components(Snapshot& snapshot,
                                          std::vector<ComponentHeader>& headers,
                                          std::vector<uint8_t>& data_section) {
  // Capture all tracked components using C API
  for (size_t i = 0; i < tracked_component_ids.size(); ++i) {
    ecs_entity_t comp_id = tracked_component_ids[i];
    size_t comp_size = tracked_component_sizes[i];

    // Create query for this component
    ecs_query_desc_t desc = {};
    desc.terms[0].id = comp_id;
    ecs_query_t* query = ecs_query_init(world->c_ptr(), &desc);

    // Iterate and capture
    ecs_iter_t it = ecs_query_iter(world->c_ptr(), query);
    while (ecs_query_next(&it)) {
      void* comp_array = ecs_field_w_size(&it, comp_size, 0);
      for (int j = 0; j < it.count; j++) {
        ecs_entity_t entity = it.entities[j];
        void* comp_data = static_cast<uint8_t*>(comp_array) + (j * comp_size);
        capture_component_op(headers, data_section, entity, comp_id, comp_data, comp_size, ComponentOp::Add);
      }
    }
    ecs_query_fini(query);
  }

  // Capture all tags (components with no data) on tracked entities
  for (flecs::entity_t entity_id : tracked_entities) {
    flecs::entity e(world->get_world(), entity_id);
    if (!e.is_alive())
      continue;

    // Iterate over all components on this entity
    e.each([&](flecs::id id) {
      // Skip pairs (relationships)
      if (id.is_pair()) {
        return;
      }

      auto component_id = id.entity();

      // Skip components we're already tracking explicitly
      if (std::find(tracked_component_ids.begin(), tracked_component_ids.end(), component_id) !=
          tracked_component_ids.end()) {
        return;
      }

      // Filter out flecs internal components
      auto flecs_module = world->lookup("flecs");
      auto identifier = world->lookup("Identifier");

      if (component_id == flecs_module.id() || component_id == identifier.id() || component_id == EcsObserver ||
          component_id == EcsQuery || component_id == EcsSystem || component_id == flecs::ChildOf ||
          component_id == EcsDependsOn) {
        return;
      }

      // Capture this tag (size 0)
      capture_component_op(headers, data_section, entity_id, component_id, nullptr, 0, ComponentOp::Add);
    });
  }
}
StateHistory::SectionData StateHistory::capture_all_relationships() {
  // For keyframes, capture relationships only on tracked entities
  auto flecs_module = world->lookup("flecs");
  auto identifier = world->lookup("Identifier");

  std::vector<RelationshipHeader> relationships;

  // Sort tracked entities for better delta encoding
  std::vector<flecs::entity_t> sorted_entities(tracked_entities.begin(), tracked_entities.end());
  std::sort(sorted_entities.begin(), sorted_entities.end());

  for (flecs::entity_t entity_id : sorted_entities) {
    flecs::entity e(world->get_world(), entity_id);
    if (!e.is_alive())
      continue;

    // Iterate over all pairs (relationships) on this entity
    e.each([&](flecs::id id) {
      // Only process pairs (relationships)
      if (!id.is_pair()) {
        return;
      }

      auto rel = id.first();
      auto tgt = id.second();

      // Skip ChildOf relationships
      if (rel == flecs::ChildOf) {
        return;
      }

      // Skip if any of these flecs-native conditions are true:
      // - Source is EcsFlecs
      // - Relation is Identifier
      // - Target is EcsObserver, EcsQuery, or EcsSystem
      // - Relation is EcsDependsOn
      if (entity_id == flecs_module.id() || rel == identifier.id() || tgt == EcsObserver || tgt == EcsQuery ||
          tgt == EcsSystem || rel == EcsDependsOn) {
        return;
      }

      // Add relationship to snapshot
      RelationshipHeader header = {};
      header.entity = entity_id;
      header.relation = rel.id();
      header.target = tgt.id();
      header.op = ComponentOp::Add;  // Keyframes store all as Add
      relationships.push_back(header);
    });
  }

  return encode_and_compress_relationships(relationships);
}
StateHistory::SectionData StateHistory::capture_changed_relationships() {
  // For diff frames, only capture relationship events that occurred
  std::cout << "  [Capturing " << relationship_events.size() << " relationship events]\n";

  std::vector<RelationshipHeader> relationships;

  for (const auto& event : relationship_events) {
    RelationshipHeader header = {};
    header.entity = event.entity;
    header.relation = event.relation;
    header.target = event.target;
    header.op = event.op;
    relationships.push_back(header);

    // Debug output - only if entities are still alive
    flecs::entity e(world->get_world(), event.entity);
    flecs::entity rel(world->get_world(), event.relation);
    flecs::entity tgt(world->get_world(), event.target);
    if (e.is_alive() && rel.is_alive() && tgt.is_alive()) {
      const char* e_name = e.name();
      const char* rel_name = rel.name();
      const char* tgt_name = tgt.name();
      std::cout << "    Captured: " << (e_name ? e_name : "unnamed") << " -[" << (rel_name ? rel_name : "unnamed")
                << "]-> " << (tgt_name ? tgt_name : "unnamed") << " (op=" << (int)event.op << ")\n";
    } else {
      std::cout << "    Captured: " << event.entity << " -[" << event.relation << "]-> " << event.target
                << " (op=" << (int)event.op << ") [destroyed]\n";
    }
  }

  return encode_and_compress_relationships(relationships);
}
StateHistory::SectionData StateHistory::encode_and_compress_relationships(
    std::vector<RelationshipHeader>& relationships) {
  if (relationships.empty()) {
    return SectionData{std::vector<uint8_t>{}, 0, false};
  }

  // Sort by entity ID for better delta encoding
  std::sort(relationships.begin(), relationships.end(),
            [](const RelationshipHeader& a, const RelationshipHeader& b) { return a.entity < b.entity; });

  // Build uncompressed buffer with delta-encoded entity IDs
  std::vector<uint8_t> uncompressed;
  uint32_t count = static_cast<uint32_t>(relationships.size());

  uncompressed.insert(uncompressed.end(), reinterpret_cast<uint8_t*>(&count),
                      reinterpret_cast<uint8_t*>(&count) + sizeof(uint32_t));

  flecs::entity_t prev_entity = 0;
  for (auto& header : relationships) {
    RelationshipHeader encoded = header;
    // Delta encode entity ID
    encoded.entity = header.entity - prev_entity;
    prev_entity = header.entity;

    uncompressed.insert(uncompressed.end(), reinterpret_cast<uint8_t*>(&encoded),
                        reinterpret_cast<uint8_t*>(&encoded) + sizeof(RelationshipHeader));
  }

  SectionData section;
  section.uncompressed_size = static_cast<uint32_t>(uncompressed.size());

  // Compress if enabled
  if (enable_compression) {
    section.data = compress_buffer(uncompressed);
    section.compressed = true;
  } else {
    section.data = std::move(uncompressed);
    section.compressed = false;
  }

  section.data.shrink_to_fit();
  return section;
}
void StateHistory::capture_changed_components(Snapshot& snapshot,
                                              std::vector<ComponentHeader>& headers,
                                              std::vector<uint8_t>& data_section) {
  // Process all component events that occurred this frame
  for (const auto& event : frame_events) {
    size_t size = registry.get_size(event.component);

    // Handle tags (components with size 0)
    if (size == 0) {
      if (event.op == ComponentOp::Remove) {
        capture_component_op(headers, data_section, event.entity, event.component, nullptr, 0, ComponentOp::Remove);
      } else if (event.op == ComponentOp::Add) {
        capture_component_op(headers, data_section, event.entity, event.component, nullptr, 0, ComponentOp::Add);
      }
      // Tags don't use Set operation
      continue;
    }

    // Get component data using C API
    if (event.op == ComponentOp::Remove) {
      // Record remove operation (no data needed)
      capture_component_op(headers, data_section, event.entity, event.component, nullptr, 0, ComponentOp::Remove);
    } else if (event.op == ComponentOp::Add) {
      // Record add operation with full data
      const void* comp = ecs_get_id(world->c_ptr(), event.entity, event.component);
      if (comp) {
        capture_component_op(headers, data_section, event.entity, event.component, comp, size, ComponentOp::Add);
      }
    } else if (event.op == ComponentOp::Set) {
      // Record set operation with XOR diff
      const void* comp = ecs_get_id(world->c_ptr(), event.entity, event.component);
      if (comp) {
        capture_component_diff(headers, data_section, event.entity, event.component, comp, size);
      }
    }
  }
}
void StateHistory::capture_component_op(std::vector<ComponentHeader>& headers,
                                        std::vector<uint8_t>& data_section,
                                        flecs::entity_t entity,
                                        flecs::entity_t component,
                                        const void* data,
                                        size_t size,
                                        ComponentOp op) {
  std::vector<uint8_t> serialized_data;
  if (data && size > 0) {
    serialized_data = registry.serialize(component, data);
  }

  ComponentHeader header = {};
  header.entity = entity;
  header.component = component;
  header.size = static_cast<uint16_t>(serialized_data.size());
  header.offset = static_cast<uint16_t>(data_section.size());
  header.op = op;

  headers.push_back(header);

  // Store data for Add and Set operations (not for Remove)
  if (data && size > 0) {
    data_section.insert(data_section.end(), serialized_data.begin(), serialized_data.end());

    // Update previous state for Add/Set operations
    auto comp_key = make_key(entity, component);
    prev_frame_state[comp_key] = serialized_data;
  } else if (op == ComponentOp::Remove) {
    // Remove from previous state cache
    auto comp_key = make_key(entity, component);
    prev_frame_state.erase(comp_key);
  }
}
void StateHistory::capture_component_diff(std::vector<ComponentHeader>& headers,
                                          std::vector<uint8_t>& data_section,
                                          flecs::entity_t entity_id,
                                          flecs::entity_t component_id,
                                          const void* data,
                                          size_t size) {
  auto comp_key = make_key(entity_id, component_id);
  std::vector<uint8_t> serialized_data = registry.serialize(component_id, data);

  auto it = prev_frame_state.find(comp_key);
  if (it == prev_frame_state.end() || it->second.size() != serialized_data.size()) {
    // Component not in cache or size changed - this shouldn't happen with proper OnSet handling,
    // or if the component size dynamically changed (though standard components don't).
    // Store as full data with Set operation
    ComponentHeader header = {};
    header.entity = entity_id;
    header.component = component_id;
    header.size = static_cast<uint16_t>(serialized_data.size());
    header.offset = static_cast<uint16_t>(data_section.size());
    header.op = ComponentOp::Set;

    headers.push_back(header);
    data_section.insert(data_section.end(), serialized_data.begin(), serialized_data.end());

    // Initialize previous state
    prev_frame_state[comp_key] = serialized_data;
    return;
  }

  const auto& prev = it->second;

  // Calculate XOR diff
  bool has_changes = false;
  for (size_t i = 0; i < serialized_data.size(); ++i) {
    if (serialized_data[i] != prev[i]) {
      has_changes = true;
      break;
    }
  }

  if (has_changes) {
    ComponentHeader header = {};
    header.entity = entity_id;
    header.component = component_id;
    header.size = static_cast<uint16_t>(serialized_data.size());
    header.offset = static_cast<uint16_t>(data_section.size());
    header.op = ComponentOp::Set;

    headers.push_back(header);

    // Store XOR diff
    for (size_t i = 0; i < serialized_data.size(); ++i) {
      data_section.push_back(serialized_data[i] ^ prev[i]);
    }

    // Update previous state
    prev_frame_state[comp_key] = serialized_data;
  }
}
void StateHistory::rollback_to(size_t target_frame) {
  if (target_frame >= snapshots.size()) {
    std::cout << "Cannot rollback to frame " << target_frame << "\n";
    return;
  }

  std::cout << "\n=== Rolling back from frame " << (current_frame - 1) << " to frame " << target_frame << " ===\n";

  // Disable event recording during rollback
  recording_enabled = false;

  // TODO: Support dynamic keyframes
  size_t keyframe_idx = (target_frame / keyframe_interval) * keyframe_interval;
  std::cout << "Jumping to keyframe " << keyframe_idx << "\n";

  // Clear all tracked components from entities before restore
  clear_all_components();
  prev_frame_state.clear();

  // Restore entities from keyframe (destroys/recreates as needed)
  restore_entities_from_keyframe(keyframe_idx);

  // Restore components from keyframe
  restore_keyframe(keyframe_idx);

  if (keyframe_idx < target_frame) {
    std::cout << "Applying events from frame " << (keyframe_idx + 1) << " to " << target_frame << "\n";
    for (size_t i = keyframe_idx + 1; i <= target_frame; ++i) {
      apply_snapshot_forward(snapshots[i]);
    }
  }

  // Re-enable event recording
  recording_enabled = true;

  // Set current frame position (preserves future timeline)
  current_frame = target_frame + 1;
  rebuild_prev_frame_state();
  frame_events.clear();
  entity_events.clear();
  relationship_events.clear();
}
void StateHistory::roll_forward(size_t target_frame) {
  if (target_frame >= snapshots.size()) {
    std::cout << "Cannot roll forward to frame " << target_frame << " (only " << snapshots.size() << " frames exist)\n";
    return;
  }

  if (target_frame < current_frame - 1) {
    std::cout << "Cannot roll forward to frame " << target_frame << " (currently at frame " << (current_frame - 1)
              << ")\n";
    return;
  }

  std::cout << "\n=== Rolling forward from frame " << (current_frame - 1) << " to frame " << target_frame << " ===\n";

  // Disable event recording during roll forward
  recording_enabled = false;

  // Find the nearest previous keyframe to target
  size_t keyframe_idx = (target_frame / keyframe_interval) * keyframe_interval;
  std::cout << "Jumping to keyframe " << keyframe_idx << "\n";

  // Clear all tracked components from entities before restore
  clear_all_components();
  prev_frame_state.clear();

  // Restore entities from keyframe (destroys/recreates as needed)
  restore_entities_from_keyframe(keyframe_idx);

  // Restore components from keyframe
  restore_keyframe(keyframe_idx);

  // Apply diffs from keyframe to target frame
  if (keyframe_idx < target_frame) {
    std::cout << "Applying events from frame " << (keyframe_idx + 1) << " to " << target_frame << "\n";
    for (size_t i = keyframe_idx + 1; i <= target_frame; ++i) {
      apply_snapshot_forward(snapshots[i]);
    }
  }

  // Re-enable event recording
  recording_enabled = true;

  // Update current frame position
  current_frame = target_frame + 1;
  rebuild_prev_frame_state();
  frame_events.clear();
  entity_events.clear();
  relationship_events.clear();
}
void StateHistory::roll_to(size_t target_frame) {
  if (current_frame < target_frame) {
    roll_forward(target_frame);
  } else if (current_frame > target_frame) {
    rollback_to(target_frame);
  }
}
void StateHistory::restore_entities_from_keyframe(size_t frame_idx) {
  if (!snapshots[frame_idx].is_keyframe()) {
    std::cerr << "Error: Frame " << frame_idx << " is not a keyframe!\n";
    return;
  }

  auto& snapshot = snapshots[frame_idx];
  auto lifecycle = snapshot.decode_entities();

  std::cout << "  Restoring entities from keyframe " << frame_idx << "\n";
  std::cout << "  Snapshot has " << lifecycle.existing_entities.size() << " entities\n";

  // Clear entity ID remap for this rollback
  entity_id_remap.clear();

  // Restore entity_names from keyframe
  entity_names.clear();
  for (const auto& [id, name] : lifecycle.entities_created) {
    entity_names[id] = name;
    std::cout << "  Restored entity name: " << id << " -> " << name << "\n";
  }

  // Get current tracked entities (entities we're managing in the history)
  std::unordered_set<flecs::entity_t> current_entities = tracked_entities;
  std::cout << "  Current tracked entities: " << current_entities.size() << "\n";

  // Destroy entities that shouldn't exist at keyframe
  for (flecs::entity_t id : current_entities) {
    if (lifecycle.existing_entities.find(id) == lifecycle.existing_entities.end()) {
      std::cout << "  Destroying entity " << id << "\n";
      flecs::entity e(world->get_world(), id);
      if (e.is_alive()) {
        std::cout << "    Entity is alive, calling destruct...\n";
        e.destruct();
        std::cout << "    Destruct completed\n";
      }
    }
  }

  std::cout << "  Destruction phase complete\n";

  // Create entities that should exist (by name) and build ID remap
  tracked_entities.clear();
  for (flecs::entity_t old_id : lifecycle.existing_entities) {
    std::string name = entity_names.count(old_id) ? entity_names[old_id] : "";
    if (!name.empty()) {
      // Get or create entity by name
      flecs::entity e = world->entity(name.c_str());
      entity_id_remap[old_id] = e.id();
      tracked_entities.insert(e.id());
      entity_names[e.id()] = name;
      std::cout << "  Recreated entity " << name << ": old ID " << old_id << " -> new ID " << e.id() << "\n";
    } else {
      std::cerr << "Warning: Entity " << old_id << " has no name, cannot recreate\n";
    }
  }

  std::cout << "  Entity restoration complete\n";
}
void StateHistory::clear_all_components() {
  // Remove all tracked components from all entities using C API
  for (ecs_entity_t comp_id : tracked_component_ids) {
    ecs_defer_begin(world->c_ptr());

    // Create query for this component
    ecs_query_desc_t desc = {};
    desc.terms[0].id = comp_id;
    ecs_query_t* query = ecs_query_init(world->c_ptr(), &desc);

    // Iterate and remove
    ecs_iter_t it = ecs_query_iter(world->c_ptr(), query);
    while (ecs_query_next(&it)) {
      for (int j = 0; j < it.count; j++) {
        ecs_remove_id(world->c_ptr(), it.entities[j], comp_id);
      }
    }
    ecs_query_fini(query);

    ecs_defer_end(world->c_ptr());
  }

  // Clear all tags from tracked entities
  world->defer_begin();
  for (flecs::entity_t entity_id : tracked_entities) {
    flecs::entity e(world->get_world(), entity_id);
    if (!e.is_alive())
      continue;

    // Iterate over all components on this entity
    std::vector<flecs::entity_t> tags_to_remove;
    e.each([&](flecs::id id) {
      // Skip pairs (relationships)
      if (id.is_pair()) {
        return;
      }

      auto component_id = id.entity();

      // Skip components we're tracking explicitly
      if (std::find(tracked_component_ids.begin(), tracked_component_ids.end(), component_id) !=
          tracked_component_ids.end()) {
        return;
      }

      // Filter out flecs internal components
      auto flecs_module = world->lookup("flecs");
      auto identifier = world->lookup("Identifier");

      if (component_id == flecs_module.id() || component_id == identifier.id() || component_id == EcsObserver ||
          component_id == EcsQuery || component_id == EcsSystem || component_id == flecs::ChildOf ||
          component_id == EcsDependsOn) {
        return;
      }

      // Mark this tag for removal
      tags_to_remove.push_back(component_id);
    });

    // Remove all tags
    for (flecs::entity_t tag : tags_to_remove) {
      e.remove(tag);
    }
  }
  world->defer_end();

  // Clear all relationships
  clear_all_relationships();
}
void StateHistory::clear_all_relationships() {
  // Remove all tracked relationships only from tracked entities
  auto flecs_module = world->lookup("flecs");
  auto identifier = world->lookup("Identifier");

  world->defer_begin();
  for (flecs::entity_t entity_id : tracked_entities) {
    flecs::entity e(world->get_world(), entity_id);
    if (!e.is_alive())
      continue;

    // Collect relationships to remove
    std::vector<std::pair<flecs::entity_t, flecs::entity_t>> rels_to_remove;

    e.each([&](flecs::id id) {
      // Only process pairs (relationships)
      if (!id.is_pair()) {
        return;
      }

      auto rel = id.first();
      auto tgt = id.second();

      // Skip ChildOf relationships
      if (rel == flecs::ChildOf) {
        return;
      }

      // Skip if any of these flecs-native conditions are true
      if (entity_id == flecs_module.id() || rel == identifier.id() || tgt == EcsObserver || tgt == EcsQuery ||
          tgt == EcsSystem || rel == EcsDependsOn) {
        return;
      }

      // Mark for removal
      rels_to_remove.push_back({rel.id(), tgt.id()});
    });

    // Remove all marked relationships
    for (const auto& [rel, tgt] : rels_to_remove) {
      e.remove(rel, tgt);
    }
  }
  world->defer_end();
}
void StateHistory::restore_keyframe(size_t frame_idx) {
  if (!snapshots[frame_idx].is_keyframe()) {
    std::cerr << "Error: Frame " << frame_idx << " is not a keyframe!\n";
    return;
  }

  auto& snapshot = snapshots[frame_idx];

  // Check total compressed size to decide if parallelization is worth it
  auto comp_info = snapshot.get_component_info();
  auto rel_info = snapshot.get_relationship_info();
  size_t total_compressed = comp_info.size + rel_info.size;

  // Only use parallel decompression if we have at least 8KB of data
  // (thread overhead isn't worth it for small snapshots)
  constexpr size_t PARALLEL_THRESHOLD = 8192;

  std::vector<uint8_t> decompressed;
  std::vector<RelationshipHeader> relationships;

  if (total_compressed >= PARALLEL_THRESHOLD) {
    // Decompress component and relationship buffers in parallel
    auto component_future =
        std::async(std::launch::async, [&snapshot]() { return snapshot.get_decompressed_buffer(); });

    auto relationship_future =
        std::async(std::launch::async, [&snapshot]() { return snapshot.decode_relationships(); });

    // Wait for decompression to complete
    decompressed = component_future.get();
    relationships = relationship_future.get();
  } else {
    // Sequential decompression for small snapshots
    decompressed = snapshot.get_decompressed_buffer();
    relationships = snapshot.decode_relationships();
  }

  if (decompressed.empty() || decompressed.size() < sizeof(uint32_t)) {
    return;
  }

  uint32_t count;
  std::memcpy(&count, decompressed.data(), sizeof(uint32_t));

  world->defer_begin();
  for (size_t i = 0; i < count; ++i) {
    const ComponentHeader* header = snapshot.get_header(i, decompressed);
    if (!header)
      continue;

    // Keyframes should only contain Add operations
    if (header->op != ComponentOp::Add) {
      std::cerr << "Warning: Non-Add operation in keyframe!\n";
      continue;
    }

    const uint8_t* data = snapshot.get_data(header, decompressed);
    if (!data)
      continue;

    // Remap entity ID if needed
    flecs::entity_t entity_id = header->entity;
    if (entity_id_remap.count(entity_id)) {
      entity_id = entity_id_remap[entity_id];
    }

    flecs::entity e(world->get_world(), entity_id);

    // Check if this is a tag (size 0)
    if (header->size == 0) {
      // This is a tag - just add it using C API
      ecs_add_id(world->c_ptr(), entity_id, header->component);
      continue;
    }

    // Restore component using C API
    size_t comp_size = registry.get_size(header->component);
    void* comp = ecs_ensure_id(world->c_ptr(), entity_id, header->component, comp_size);
    if (comp) {
      std::vector<uint8_t> buffer(data, data + header->size);
      registry.deserialize(header->component, comp, buffer);
      ecs_modified_id(world->c_ptr(), entity_id, header->component);
      auto comp_key = make_key(entity_id, header->component);
      prev_frame_state[comp_key] = buffer;
    }
  }
  world->defer_end();

  // Restore relationships
  world->defer_begin();
  for (const auto& rel_header : relationships) {
    // Remap entity ID if needed
    flecs::entity_t entity_id = rel_header.entity;
    if (entity_id_remap.count(entity_id)) {
      entity_id = entity_id_remap[entity_id];
    }

    // Remap target ID if needed
    flecs::entity_t target_id = rel_header.target;
    if (entity_id_remap.count(target_id)) {
      target_id = entity_id_remap[target_id];
    }

    flecs::entity e(world->get_world(), entity_id);
    flecs::entity target(world->get_world(), target_id);

    // Add the relationship
    e.add(rel_header.relation, target_id);
  }
  world->defer_end();
}
void StateHistory::apply_snapshot_forward(Snapshot& snapshot) {
  // Check total compressed size to decide if parallelization is worth it
  auto comp_info = snapshot.get_component_info();
  auto ent_info = snapshot.get_entity_info();
  auto rel_info = snapshot.get_relationship_info();
  size_t total_compressed = comp_info.size + ent_info.size + rel_info.size;

  // Only use parallel decompression if we have at least 8KB of data
  // (thread overhead isn't worth it for small differential snapshots)
  constexpr size_t PARALLEL_THRESHOLD = 8192;

  std::vector<RelationshipHeader> relationships;
  Snapshot::EntityLifecycle lifecycle;
  std::vector<uint8_t> decompressed;

  if (total_compressed >= PARALLEL_THRESHOLD) {
    // Launch all three decompressions in parallel
    auto relationship_future =
        std::async(std::launch::async, [&snapshot]() { return snapshot.decode_relationships(); });

    auto entity_future = std::async(std::launch::async, [&snapshot]() { return snapshot.decode_entities(); });

    auto component_future =
        std::async(std::launch::async, [&snapshot]() { return snapshot.get_decompressed_buffer(); });

    // Wait for decompressions to complete
    relationships = relationship_future.get();
    lifecycle = entity_future.get();
    decompressed = component_future.get();
  } else {
    // Sequential decompression for small snapshots
    relationships = snapshot.decode_relationships();
    lifecycle = snapshot.decode_entities();
    decompressed = snapshot.get_decompressed_buffer();
  }

  std::cout << "  Applying frame " << snapshot.frame << " (relationships: " << relationships.size() << ")\n";

  // Track which entities are destroyed in this frame to skip their component
  // operations
  std::unordered_set<flecs::entity_t> destroyed_this_frame;

  if (snapshot.is_keyframe()) {
    restore_entities_from_keyframe(snapshot.frame);
    restore_keyframe(snapshot.frame);
    std::cout << "  [DEBUG] Keyframe restored, now will apply relationships\n";
    // Fall through to relationship application at the end
  } else {
    std::cout << "  [DEBUG] Not a keyframe, processing diff frame\n";

    // Apply entity lifecycle events first
    for (const auto& [old_id, name] : lifecycle.entities_created) {
      if (!name.empty()) {
        flecs::entity e = world->entity(name.c_str());
        // Add to remap
        entity_id_remap[old_id] = e.id();
        tracked_entities.insert(e.id());
        entity_names[e.id()] = name;
        std::cout << "    Created entity " << name << ": old ID " << old_id << " -> new ID " << e.id() << "\n";
      } else {
        std::cerr << "Warning: Cannot create unnamed entity during roll forward\n";
      }
    }

    for (flecs::entity_t old_id : lifecycle.entities_destroyed) {
      // Remap ID if needed
      flecs::entity_t id = old_id;
      if (entity_id_remap.count(old_id)) {
        id = entity_id_remap[old_id];
      }

      destroyed_this_frame.insert(id);

      flecs::entity e(world->get_world(), id);
      if (e.is_alive()) {
        std::cout << "    Destroying entity: old ID " << old_id << " -> new ID " << id << "\n";
        e.destruct();
      }
      tracked_entities.erase(id);
      entity_names.erase(id);
    }

    if (decompressed.empty() || decompressed.size() < sizeof(uint32_t)) {
      std::cout << "  [DEBUG] Buffer is empty, but will apply relationships at end\n";
      // Don't return here - fall through to relationship application
    } else {
      uint32_t count;
      std::memcpy(&count, decompressed.data(), sizeof(uint32_t));

      world->defer_begin();
      for (size_t i = 0; i < count; ++i) {
        const ComponentHeader* header = snapshot.get_header(i, decompressed);
        if (!header)
          continue;

        // Remap entity ID if needed
        flecs::entity_t entity_id = header->entity;
        if (entity_id_remap.count(entity_id)) {
          entity_id = entity_id_remap[entity_id];
        }

        // Skip component operations for entities destroyed in this frame
        if (destroyed_this_frame.count(entity_id)) {
          continue;
        }

        flecs::entity e(world->get_world(), entity_id);

        // Check if this is a tag (size 0)
        if (header->size == 0) {
          if (header->op == ComponentOp::Add) {
            ecs_add_id(world->c_ptr(), entity_id, header->component);
          } else if (header->op == ComponentOp::Remove) {
            ecs_remove_id(world->c_ptr(), entity_id, header->component);
          }
          continue;
        }

        // Handle operation based on type using C API
        size_t comp_size = registry.get_size(header->component);
        if (header->op == ComponentOp::Add) {
          // Add component with data
          const uint8_t* data = snapshot.get_data(header, decompressed);
          if (data) {
            void* comp = ecs_ensure_id(world->c_ptr(), entity_id, header->component, comp_size);
            if (comp) {
              std::vector<uint8_t> buffer(data, data + header->size);
              registry.deserialize(header->component, comp, buffer);
              ecs_modified_id(world->c_ptr(), entity_id, header->component);
              auto comp_key = make_key(entity_id, header->component);
              prev_frame_state[comp_key] = buffer;
            }
          }
        } else if (header->op == ComponentOp::Remove) {
          // Remove component
          ecs_remove_id(world->c_ptr(), entity_id, header->component);
          auto comp_key = make_key(entity_id, header->component);
          prev_frame_state.erase(comp_key);
        } else if (header->op == ComponentOp::Set) {
          // Apply XOR diff
          const uint8_t* diff_data = snapshot.get_data(header, decompressed);
          if (diff_data) {
            void* comp = ecs_ensure_id(world->c_ptr(), entity_id, header->component, comp_size);
            if (comp) {
              auto comp_key = make_key(entity_id, header->component);
              auto& prev = prev_frame_state[comp_key];

              if (prev.size() != header->size) {
                prev.resize(header->size);
              }

              for (size_t j = 0; j < header->size; ++j) {
                prev[j] ^= diff_data[j];
              }

              registry.deserialize(header->component, comp, prev);
              ecs_modified_id(world->c_ptr(), entity_id, header->component);
            }
          }
        }
      }
      std::cout << "  [DEBUG] About to call defer_end for components\n";
      world->defer_end();
      std::cout << "  [DEBUG] defer_end completed\n";
    }  // end else block for decompressed buffer check
  }  // end else block for keyframe check

  // Apply relationship changes (for both keyframes and diff frames) - use
  // already decoded relationships
  std::cout << "  [DEBUG] About to apply relationships, count: " << relationships.size() << "\n";
  if (!relationships.empty()) {
    std::cout << "    Applying " << relationships.size() << " relationship changes\n";
  }
  world->defer_begin();
  for (const auto& rel_header : relationships) {
    // Remap entity ID if needed
    flecs::entity_t entity_id = rel_header.entity;
    if (entity_id_remap.count(entity_id)) {
      entity_id = entity_id_remap[entity_id];
    }

    // Remap target ID if needed
    flecs::entity_t target_id = rel_header.target;
    if (entity_id_remap.count(target_id)) {
      target_id = entity_id_remap[target_id];
    }

    // Skip relationship operations for entities destroyed in this frame
    if (destroyed_this_frame.count(entity_id)) {
      continue;
    }

    flecs::entity e(world->get_world(), entity_id);
    flecs::entity rel(world->get_world(), rel_header.relation);
    flecs::entity tgt(world->get_world(), target_id);

    if (rel_header.op == ComponentOp::Add) {
      // Add the relationship
      if (e.is_alive() && rel.is_alive() && tgt.is_alive()) {
        const char* e_name = e.name();
        const char* rel_name = rel.name();
        const char* tgt_name = tgt.name();
        std::cout << "      Adding relationship: " << (e_name ? e_name : "unnamed") << " -["
                  << (rel_name ? rel_name : "unnamed") << "]-> " << (tgt_name ? tgt_name : "unnamed") << "\n";
      }
      e.add(rel_header.relation, target_id);
    } else if (rel_header.op == ComponentOp::Remove) {
      // Remove the relationship
      if (e.is_alive() && rel.is_alive() && tgt.is_alive()) {
        const char* e_name = e.name();
        const char* rel_name = rel.name();
        const char* tgt_name = tgt.name();
        std::cout << "      Removing relationship: " << (e_name ? e_name : "unnamed") << " -["
                  << (rel_name ? rel_name : "unnamed") << "]-> " << (tgt_name ? tgt_name : "unnamed") << "\n";
      }
      e.remove(rel_header.relation, target_id);
    }
  }
  world->defer_end();
}

void StateHistory::rebuild_prev_frame_state() {
  prev_frame_state.clear();

  // Rebuild state for all tracked components using C API
  for (size_t i = 0; i < tracked_component_ids.size(); ++i) {
    ecs_entity_t comp_id = tracked_component_ids[i];
    size_t comp_size = tracked_component_sizes[i];

    // Create query for this component
    ecs_query_desc_t desc = {};
    desc.terms[0].id = comp_id;
    ecs_query_t* query = ecs_query_init(world->c_ptr(), &desc);

    // Iterate and cache
    ecs_iter_t it = ecs_query_iter(world->c_ptr(), query);
    while (ecs_query_next(&it)) {
      void* comp_array = ecs_field_w_size(&it, comp_size, 0);
      for (int j = 0; j < it.count; j++) {
        ecs_entity_t entity = it.entities[j];
        // Ensure entity is tracked before caching component
        if (tracked_entities.find(entity) != tracked_entities.end()) {
            void* comp_data = static_cast<uint8_t*>(comp_array) + (j * comp_size);

            auto comp_key = make_key(entity, comp_id);
            std::vector<uint8_t> serialized_data = registry.serialize(comp_id, comp_data);
            prev_frame_state[comp_key] = serialized_data;
        }
      }
    }
    ecs_query_fini(query);
  }
}

void StateHistory::print_stats() {
  size_t keyframe_data_bytes = 0;
  size_t diff_data_bytes = 0;
  size_t keyframe_compressed_bytes = 0;
  size_t diff_compressed_bytes = 0;
  size_t keyframe_count = 0;
  size_t total_component_states = 0;
  size_t total_diffs_stored = 0;
  size_t empty_diff_frames = 0;

  size_t keyframe_rel_uncompressed = 0;
  size_t keyframe_rel_compressed = 0;
  size_t diff_rel_uncompressed = 0;
  size_t diff_rel_compressed = 0;
  size_t total_relationships = 0;

  size_t total_entities_created = 0;
  size_t total_entities_destroyed = 0;
  size_t keyframe_entities = 0;
  size_t entity_lifecycle_memory = 0;

  size_t total_memory = 0;
  size_t snapshot_overhead = 0;
  size_t prev_state_memory = 0;

  for (const auto& snapshot : snapshots) {
    total_memory += snapshot.memory_size();
    snapshot_overhead += sizeof(Snapshot);

    auto decompressed = snapshot.get_decompressed_buffer();
    if (decompressed.empty())
      continue;

    uint32_t count;
    std::memcpy(&count, decompressed.data(), sizeof(uint32_t));

    if (snapshot.is_keyframe()) {
      keyframe_count++;

      auto comp_info = snapshot.get_component_info();
      keyframe_data_bytes += comp_info.uncompressed_size;
      keyframe_compressed_bytes += comp_info.size;
      total_component_states += count;

      // Relationship stats for keyframes
      auto rel_info = snapshot.get_relationship_info();
      keyframe_rel_compressed += rel_info.size;
      keyframe_rel_uncompressed += rel_info.uncompressed_size;
    } else {
      auto comp_info = snapshot.get_component_info();
      diff_data_bytes += comp_info.uncompressed_size;
      diff_compressed_bytes += comp_info.size;

      if (count == 0)
        empty_diff_frames++;
      total_diffs_stored += count;

      // Relationship stats for diff frames
      auto rel_info = snapshot.get_relationship_info();
      diff_rel_compressed += rel_info.size;
      diff_rel_uncompressed += rel_info.uncompressed_size;
    }

    // Count relationships
    auto rels = snapshot.decode_relationships();
    total_relationships += rels.size();

    // Count entity lifecycle events
    auto lifecycle = snapshot.decode_entities();
    total_entities_created += lifecycle.entities_created.size();
    total_entities_destroyed += lifecycle.entities_destroyed.size();
    if (snapshot.is_keyframe()) {
      keyframe_entities += lifecycle.existing_entities.size();
    }

    // Calculate entity lifecycle memory (compressed size)
    auto ent_info = snapshot.get_entity_info();
    entity_lifecycle_memory += ent_info.size;
  }

  for (const auto& [key, data] : prev_frame_state) {
    prev_state_memory += sizeof(key) + sizeof(std::vector<uint8_t>) + data.capacity();
  }
}

void TimelineTree::roll_to(TimelineNode* node, size_t target_frame) {
  if (node->parent == nullptr)  // root node
  {
    node->history->roll_to(target_frame);
  } else {
    // Start from the snapshot of the parent at branchframe
    node->parent->history->roll_to(node->branch_frame);
    if (target_frame > 0) {
      node->history->entity_id_remap = node->parent->history->entity_id_remap;
      // Non root nodes are offset by 1 such that the 0th index is the
      // first delta from their source made into a full keyframe
      node->history->roll_to(target_frame - 1);
    }
  }
}
