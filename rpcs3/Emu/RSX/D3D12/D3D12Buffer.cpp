#include "stdafx.h"
#include "stdafx_d3d12.h"
#ifdef _MSC_VER

#include "D3D12GSRender.h"
#include "d3dx12.h"
#include "../Common/BufferUtils.h"
#include "D3D12Formats.h"
#include "../rsx_methods.h"


std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> D3D12GSRender::upload_vertex_attributes(
	const std::vector<std::pair<u32, u32> > &vertex_ranges,
	gsl::not_null<ID3D12GraphicsCommandList*> command_list)
{
	std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> vertex_buffer_views;
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertex_buffer_data.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
	size_t input_slot = 0;

	size_t vertex_count = 0;

	size_t offset_in_vertex_buffers_buffer = 0;

	for (const auto &pair : vertex_ranges)
		vertex_count += pair.second;

	u32 input_mask = rsx::method_registers[NV4097_SET_VERTEX_ATTRIB_INPUT_MASK];

	for (int index = 0; index < rsx::limits::vertex_count; ++index)
	{
		bool enabled = !!(input_mask & (1 << index));
		if (!enabled)
			continue;

		if (vertex_arrays_info[index].size > 0)
		{
			// Active vertex array
			const rsx::data_array_format_info &info = vertex_arrays_info[index];

			size_t element_size = rsx::get_vertex_type_size_on_host(info.type, info.size);
			size_t buffer_size = element_size * vertex_count;

			size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);

			void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
			for (const auto &range : vertex_ranges)
			{
				write_vertex_array_data_to_buffer(mapped_buffer, range.first, range.second, index, info);
				mapped_buffer = (char*)mapped_buffer + range.second * element_size;
			}
			m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));

			command_list->CopyBufferRegion(m_vertex_buffer_data.Get(), offset_in_vertex_buffers_buffer, m_buffer_data.get_heap(), heap_offset, buffer_size);

			UINT component_mapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			switch (info.size)
			{
			case 1:
				component_mapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
					D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
					D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0,
					D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0,
					D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
					);
					break;
			case 2:
				component_mapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
					D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
					D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
					D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0,
					D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
					);
				break;
			case 3:
				component_mapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
					D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
					D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
					D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
					D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
					);
				break;
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC vertex_buffer_view = {
				get_vertex_attribute_format(info.type, info.size),
				D3D12_SRV_DIMENSION_BUFFER,
				component_mapping
			};
			vertex_buffer_view.Buffer.FirstElement = offset_in_vertex_buffers_buffer / element_size;
			vertex_buffer_view.Buffer.NumElements = buffer_size / element_size;
			vertex_buffer_views.push_back(vertex_buffer_view);

			offset_in_vertex_buffers_buffer = (offset_in_vertex_buffers_buffer + buffer_size + 191) / 192; // 192 is multiple of 2, 4, 6, 8, 12, 16, 24, 32, 48, 64
			offset_in_vertex_buffers_buffer *= 192;

			m_timers.m_buffer_upload_size += buffer_size;

		}
		else if (register_vertex_info[index].size > 0)
		{
			// In register vertex attribute
			const rsx::data_array_format_info &info = register_vertex_info[index];

			const std::vector<u8> &data = register_vertex_data[index];

			size_t element_size = rsx::get_vertex_type_size_on_host(info.type, info.size);
			size_t buffer_size = data.size();

			size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);

			void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
			memcpy(mapped_buffer, data.data(), data.size());
			m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));

			command_list->CopyBufferRegion(m_vertex_buffer_data.Get(), offset_in_vertex_buffers_buffer, m_buffer_data.get_heap(), heap_offset, buffer_size);

			D3D12_SHADER_RESOURCE_VIEW_DESC vertex_buffer_view = {
				get_vertex_attribute_format(info.type, info.size),
				D3D12_SRV_DIMENSION_BUFFER,
				D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
			};
			vertex_buffer_view.Buffer.FirstElement = offset_in_vertex_buffers_buffer / element_size;
			vertex_buffer_view.Buffer.NumElements = buffer_size / element_size;
			vertex_buffer_views.push_back(vertex_buffer_view);

			offset_in_vertex_buffers_buffer = (offset_in_vertex_buffers_buffer + buffer_size + 191) / 192; // 192 is multiple of 2, 4, 6, 8, 12, 16, 24, 32, 48, 64
			offset_in_vertex_buffers_buffer *= 192;
		}
	}
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertex_buffer_data.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	return vertex_buffer_views;
}

void D3D12GSRender::upload_and_bind_scale_offset_matrix(size_t descriptorIndex)
{
	size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(256);

	// Scale offset buffer
	// Separate constant buffer
	void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + 256));
	fill_scale_offset_data(mapped_buffer);
	int is_alpha_tested = !!(rsx::method_registers[NV4097_SET_ALPHA_TEST_ENABLE]);
	float alpha_ref = (float&)rsx::method_registers[NV4097_SET_ALPHA_REF];
	memcpy((char*)mapped_buffer + 16 * sizeof(float), &is_alpha_tested, sizeof(int));
	memcpy((char*)mapped_buffer + 17 * sizeof(float), &alpha_ref, sizeof(float));

	size_t tex_idx = 0;
	for (u32 i = 0; i < rsx::limits::textures_count; ++i)
	{
		if (!textures[i].enabled())
		{
			int is_unorm = false;
			memcpy((char*)mapped_buffer + (18 + tex_idx++) * sizeof(int), &is_unorm, sizeof(int));
			continue;
		}
		size_t w = textures[i].width(), h = textures[i].height();
//		if (!w || !h) continue;

		int is_unorm = (textures[i].format() & CELL_GCM_TEXTURE_UN);
		memcpy((char*)mapped_buffer + (18 + tex_idx++) * sizeof(int), &is_unorm, sizeof(int));
	}
	m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + 256));

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {
		m_buffer_data.get_heap()->GetGPUVirtualAddress() + heap_offset,
		256
	};
	m_device->CreateConstantBufferView(&constant_buffer_view_desc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetCPUDescriptorHandleForHeapStart())
		.Offset((INT)descriptorIndex, g_descriptor_stride_srv_cbv_uav));
}

void D3D12GSRender::upload_and_bind_vertex_shader_constants(size_t descriptor_index)
{
	size_t buffer_size = 512 * 4 * sizeof(float);

	size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);

	void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	fill_vertex_program_constants_data(mapped_buffer);
	m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {
		m_buffer_data.get_heap()->GetGPUVirtualAddress() + heap_offset,
		(UINT)buffer_size
	};
	m_device->CreateConstantBufferView(&constant_buffer_view_desc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetCPUDescriptorHandleForHeapStart())
		.Offset((INT)descriptor_index, g_descriptor_stride_srv_cbv_uav));
}

void D3D12GSRender::upload_and_bind_fragment_shader_constants(size_t descriptor_index)
{
	// Get constant from fragment program
	size_t buffer_size = m_pso_cache.get_fragment_constants_buffer_size(fragment_program);
	// Multiple of 256 never 0
	buffer_size = (buffer_size + 255) & ~255;

	size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);

	size_t offset = 0;
	float *mapped_buffer = m_buffer_data.map<float>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	m_pso_cache.fill_fragment_constans_buffer({ mapped_buffer, gsl::narrow<int>(buffer_size) }, fragment_program);
	m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {
		m_buffer_data.get_heap()->GetGPUVirtualAddress() + heap_offset,
		(UINT)buffer_size
	};
	m_device->CreateConstantBufferView(&constant_buffer_view_desc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetCPUDescriptorHandleForHeapStart())
		.Offset((INT)descriptor_index, g_descriptor_stride_srv_cbv_uav));
}


std::tuple<D3D12_VERTEX_BUFFER_VIEW, size_t> D3D12GSRender::upload_inlined_vertex_array()
{
	UINT offset = 0;

	// Bind attributes
	for (int index = 0; index < rsx::limits::vertex_count; ++index)
	{
		const auto &info = vertex_arrays_info[index];

		if (!info.size) // disabled
			continue;

		offset += rsx::get_vertex_type_size_on_host(info.type, info.size);
	}

	// Copy inline buffer
	size_t buffer_size = inline_vertex_array.size() * sizeof(int);
	size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);
	void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	write_inline_array_to_buffer(mapped_buffer);
	m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));

	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view =
	{
		m_buffer_data.get_heap()->GetGPUVirtualAddress() + heap_offset,
		(UINT)buffer_size,
		(UINT)offset
	};

	return std::make_tuple(vertex_buffer_view, (u32)buffer_size / offset);
}

std::tuple<D3D12_INDEX_BUFFER_VIEW, size_t> D3D12GSRender::generate_index_buffer_for_emulated_primitives_array(const std::vector<std::pair<u32, u32> > &vertex_ranges)
{
	size_t index_count = 0;
	for (const auto &pair : vertex_ranges)
		index_count += get_index_count(draw_mode, pair.second);

	// Alloc
	size_t buffer_size = align(index_count * sizeof(u16), 64);
	size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);

	void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	size_t first = 0;
	for (const auto &pair : vertex_ranges)
	{
		size_t element_count = get_index_count(draw_mode, pair.second);
		write_index_array_for_non_indexed_non_native_primitive_to_buffer((char*)mapped_buffer, draw_mode, (u32)first, (u32)pair.second);
		mapped_buffer = (char*)mapped_buffer + element_count * sizeof(u16);
		first += pair.second;
	}
	m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	D3D12_INDEX_BUFFER_VIEW index_buffer_view = {
		m_buffer_data.get_heap()->GetGPUVirtualAddress() + heap_offset,
		(UINT)buffer_size,
		DXGI_FORMAT_R16_UINT
	};

	return std::make_tuple(index_buffer_view, index_count);
}

std::tuple<bool, size_t, std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC>> D3D12GSRender::upload_and_set_vertex_index_data(ID3D12GraphicsCommandList *command_list)
{
	if (draw_command == Draw_command::draw_command_inlined_array)
	{
/*		size_t vertex_count;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		std::tie(vertex_buffer_view, vertex_count) = upload_inlined_vertex_array();
		command_list->IASetVertexBuffers(0, (UINT)1, &vertex_buffer_view);

		if (is_primitive_native(draw_mode))
			return std::make_tuple(false, vertex_count);

		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		size_t index_count;
		std::tie(index_buffer_view, index_count) = generate_index_buffer_for_emulated_primitives_array({ { 0, (u32)vertex_count } });
		command_list->IASetIndexBuffer(&index_buffer_view);
		return std::make_tuple(true, index_count);*/
	}

	if (draw_command == Draw_command::draw_command_array)
	{
		if (is_primitive_native(draw_mode))
		{
			// Index count
			size_t vertex_count = 0;
			for (const auto &pair : first_count_commands)
				vertex_count += pair.second;
			return std::make_tuple(false, vertex_count, upload_vertex_attributes(first_count_commands, command_list));
		}

		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		size_t index_count;
		std::tie(index_buffer_view, index_count) = generate_index_buffer_for_emulated_primitives_array(first_count_commands);
		command_list->IASetIndexBuffer(&index_buffer_view);
		return std::make_tuple(true, index_count, upload_vertex_attributes(first_count_commands, command_list));
	}

	assert(draw_command == Draw_command::draw_command_indexed);

	// Index count
	size_t index_count = 0;
	for (const auto &pair : first_count_commands)
		index_count += pair.second;
	index_count = get_index_count(draw_mode, gsl::narrow<int>(index_count));

	Index_array_type indexed_type = to_index_array_type(rsx::method_registers[NV4097_SET_INDEX_ARRAY_DMA] >> 4);
	size_t index_size = get_index_type_size(indexed_type);

	// Alloc
	size_t buffer_size = align(index_count * index_size, 64);
	size_t heap_offset = m_buffer_data.alloc<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(buffer_size);

	void *mapped_buffer = m_buffer_data.map<void>(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	u32 min_index, max_index;

	if (indexed_type == Index_array_type::unsigned_16b)
	{
		gsl::span<u16> dst = { (u16*)mapped_buffer, gsl::narrow<int>(buffer_size / index_size) };
		std::tie(min_index, max_index) = write_index_array_data_to_buffer(dst, draw_mode, first_count_commands);
	}

	if (indexed_type == Index_array_type::unsigned_32b)
	{
		gsl::span<u32> dst = { (u32*)mapped_buffer, gsl::narrow<int>(buffer_size / index_size) };
		std::tie(min_index, max_index) = write_index_array_data_to_buffer(dst, draw_mode, first_count_commands);
	}

	m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + buffer_size));
	D3D12_INDEX_BUFFER_VIEW index_buffer_view = {
		m_buffer_data.get_heap()->GetGPUVirtualAddress() + heap_offset,
		(UINT)buffer_size,
		get_index_type(indexed_type)
	};
	m_timers.m_buffer_upload_size += buffer_size;
	command_list->IASetIndexBuffer(&index_buffer_view);

	return std::make_tuple(true, index_count, upload_vertex_attributes({ std::make_pair(0, max_index + 1) }, command_list));
}

#endif
