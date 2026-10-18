//
//  EmptyStateViews.swift
//  Orlix
//

import SwiftUI

// MARK: - Empty State Views

package struct ServerConnectEmptyState: View {
    package let server: Server
    package let onConnect: () -> Void
    @Environment(\.privacyModeEnabled) private var privacyModeEnabled

    package var body: some View {
        VStack(spacing: 24) {
            Spacer()

            VStack(spacing: 16) {
                Image(systemName: "server.rack")
                    .font(.system(size: 56))
                    .foregroundStyle(.secondary)

                VStack(spacing: 8) {
                    Text(server.name)
                        .font(.title2)
                        .fontWeight(.semibold)

                    Text(server.visibleAddress(privacyModeEnabled: privacyModeEnabled))
                        .font(.body)
                        .foregroundStyle(.secondary)
                }
            }

            Button(action: onConnect) {
                HStack(spacing: 8) {
                    Image(systemName: "terminal.fill")
                    Text("Connect")
                        .fontWeight(.medium)
                }
                .padding(.horizontal, 24)
                .padding(.vertical, 12)
                .background(.tint, in: RoundedRectangle(cornerRadius: 10))
                .foregroundStyle(.white)
            }
            .buttonStyle(.plain)

            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

package struct NoServerSelectedEmptyState: View {
    package var body: some View {
        VStack(spacing: 24) {
            Spacer()

            VStack(spacing: 16) {
                Image(systemName: "arrow.left.circle")
                    .font(.system(size: 56))
                    .foregroundStyle(.tertiary)

                VStack(spacing: 8) {
                    Text("Select a Server")
                        .font(.title2)
                        .fontWeight(.semibold)

                    Text("Choose a server from the sidebar to connect")
                        .font(.body)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
            }

            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

package struct NoServersEmptyState: View {
    package let onAddServer: () -> Void
    package var onAddWorkspace: (() -> Void)? = nil
    package var requiresWorkspace = false

    package var body: some View {
        VStack(spacing: 24) {
            VStack(spacing: 16) {
                if requiresWorkspace {
                    Image(systemName: "folder")
                        .font(.system(size: 48))
                        .foregroundStyle(.tertiary)

                    VStack(spacing: 8) {
                        Text("No Workspaces")
                            .font(.headline)
                            .fontWeight(.semibold)

                        Text("Create a workspace before adding servers")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                } else {
                    Image(systemName: "server.rack")
                        .font(.system(size: 48))
                        .foregroundStyle(.tertiary)

                    VStack(spacing: 8) {
                        Text("No Servers")
                            .font(.headline)
                            .fontWeight(.semibold)

                        Text("Add a server to get started")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                }
            }

            if let onAddWorkspace, requiresWorkspace {
                Button(action: onAddWorkspace) {
                    HStack(spacing: 8) {
                        Image(systemName: "folder.badge.plus")
                        Text("Create Workspace")
                            .fontWeight(.medium)
                    }
                    .padding(.horizontal, 20)
                    .padding(.vertical, 10)
                    .background(.tint, in: RoundedRectangle(cornerRadius: 8))
                    .foregroundStyle(.white)
                }
                .buttonStyle(.plain)
            } else {
                Button(action: onAddServer) {
                    HStack(spacing: 8) {
                        Image(systemName: "plus.circle.fill")
                        Text("Add Server")
                            .fontWeight(.medium)
                    }
                    .padding(.horizontal, 20)
                    .padding(.vertical, 10)
                    .background(.tint, in: RoundedRectangle(cornerRadius: 8))
                    .foregroundStyle(.white)
                }
                .buttonStyle(.plain)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 32)
    }
}
