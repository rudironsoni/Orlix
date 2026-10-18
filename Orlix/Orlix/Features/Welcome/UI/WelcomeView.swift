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
}

#Preview {
    WelcomeView(hasSeenWelcome: .constant(false), onCompleted: {})
}
