#if os(iOS)
import SwiftUI

package extension CustomThemeSaveSheet {
    package func platformThemeNameField(name: Binding<String>) -> some View {
        HStack(spacing: 10) {
            Text("Name")
            Spacer(minLength: 8)
            TextField("", text: name, prompt: Text("Custom Theme"))
                .multilineTextAlignment(.trailing)
        }
    }

    package var platformBody: some View {
        NavigationView {
            formContent
                .navigationTitle("Save Custom Theme")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .cancellationAction) {
                        Button("Cancel") { dismiss() }
                    }
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Save") {
                            save()
                        }
                        .disabled(!canSave)
                    }
                }
        }
        .adaptiveSoftScrollEdges()
    }
}
#endif
