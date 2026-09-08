//
//  GhosttyHostIO.swift
//  Orlix
//
//  Host-managed terminal I/O. libghostty-spm uses write_buffer and
//  receive_buffer. The vendored GhosttyKit uses feed_data and
//  set_write_callback.
//

import Foundation

enum GhosttyHostIO {
    static func feed(_ surface: ghostty_surface_t, _ data: Data) {
        guard !data.isEmpty else { return }
        data.withUnsafeBytes { buffer in
            guard let ptr = buffer.baseAddress?.assumingMemoryBound(to: UInt8.self) else { return }
            #if ORLIX_GHOSTTY_SPM
            ghostty_surface_write_buffer(surface, ptr, UInt(buffer.count))
            #else
            ghostty_surface_feed_data(surface, ptr, buffer.count)
            #endif
        }
    }

    static func setWriteCallback(
        _ surface: ghostty_surface_t,
        _ callback: Ghostty.Surface.WriteCallback?,
        userdata: UnsafeMutableRawPointer?
    ) {
        #if ORLIX_GHOSTTY_SPM
        _ = callback
        _ = userdata
        _ = surface
        #else
        ghostty_surface_set_write_callback(surface, callback, userdata)
        #endif
    }

    static func applyHostManagedIO(
        _ config: inout ghostty_surface_config_s,
        enabled: Bool,
        userdata: UnsafeMutableRawPointer?
    ) {
        #if ORLIX_GHOSTTY_SPM
        if enabled {
            config.backend = GHOSTTY_SURFACE_IO_BACKEND_HOST_MANAGED
            config.receive_buffer = ghosttyTerminalWriteCallback
            config.receive_userdata = userdata
        } else {
            config.backend = GHOSTTY_SURFACE_IO_BACKEND_EXEC
        }
        #else
        config.use_custom_io = enabled
        #endif
    }
}
