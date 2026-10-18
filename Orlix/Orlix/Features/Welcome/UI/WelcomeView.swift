//
//  WelcomeView.swift
//  Orlix
//

import SwiftUI

package struct WelcomeView: View {
    @Binding package var hasSeenWelcome: Bool
    package let onCompleted: () -> Void

    package var body: some View {
        platformContent
    }


    package init(

        hasSeenWelcome: Binding<Bool>,
        onCompleted: @escaping () -> Void
    ) {
        self._hasSeenWelcome = hasSeenWelcome

        self.onCompleted = onCompleted

    }
}

#Preview {
    WelcomeView(hasSeenWelcome: .constant(false), onCompleted: {})
}
