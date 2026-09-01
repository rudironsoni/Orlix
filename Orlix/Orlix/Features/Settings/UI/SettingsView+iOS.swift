#if os(iOS)
import SwiftUI

extension SettingsView {
    var platformBody: some View {
        NavigationView {
            List {
                ForEach(visibleRoutes(from: SettingsRouteCatalog.leadingRoutes)) { route in
                    NavigationLink {
                        settingsDestination(for: route)
                    } label: {
                        routeLabel(for: route)
                    }
                }

                ForEach(SettingsRouteCatalog.groups) { group in
                    let routes = visibleRoutes(in: group)
                    if !routes.isEmpty {
                        Section(group.title) {
                            ForEach(routes) { route in
                                NavigationLink {
                                    settingsDestination(for: route)
                                } label: {
                                    routeLabel(for: route)
                                }
                            }
                        }
                    }
                }

                ForEach(visibleRoutes(from: SettingsRouteCatalog.trailingRoutes)) { route in
                    NavigationLink {
                        settingsDestination(for: route)
                    } label: {
                        routeLabel(for: route)
                    }
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .searchable(text: $searchText, prompt: Text("Search Settings"))
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        dismiss()
                    } label: {
                        Image(systemName: "xmark")
                            .font(.system(size: 16, weight: .semibold))
                            .symbolRenderingMode(.hierarchical)
                            .foregroundStyle(.secondary)
                    }
                    .accessibilityLabel("Close")
                    .accessibilityIdentifier("orlix.settings.close")
                }
            }
        }
        .navigationViewStyle(.stack)
        .adaptiveSoftScrollEdges()
        .accessibilityIdentifier("orlix.settings.root")
    }

    private func settingsDestination(for route: SettingsRoute) -> some View {
        destination(for: route)
            .navigationTitle(route.title)
            .navigationBarTitleDisplayMode(.inline)
            .adaptiveSoftScrollEdges()
    }

}
#endif
