//
//  GhosttyTerminalMouseCapturePresentation+iOS.swift
//  Orlix
//
//  iOS per-surface mouse reporting override and feedback.
//

#if os(iOS)
import UIKit

extension GhosttyTerminalView {
    @discardableResult
    func setMouseReportingSuppressed(
        _ isSuppressed: Bool,
        showFeedback: Bool = false
    ) -> Bool {
        guard isSuppressed != isMouseReportingSuppressed else {
            keyboardToolbar?.setMouseReportingSuppressed(isSuppressed)
            return true
        }
        guard surface?.perform(action: "toggle_mouse_reporting") == true else {
            return false
        }

        isMouseReportingSuppressed = isSuppressed
        keyboardToolbar?.setMouseReportingSuppressed(isSuppressed)
        onMouseReportingSuppressionChange?(isSuppressed)
        if showFeedback {
            showMouseCaptureIndicator(isSuppressed: isSuppressed)
        }
        return true
    }

    func toggleMouseReportingSuppression() {
        _ = setMouseReportingSuppressed(
            !isMouseReportingSuppressed,
            showFeedback: true
        )
    }

    private func showMouseCaptureIndicator(isSuppressed: Bool) {
        let message = isSuppressed
            ? String(localized: "Mouse Capture Off")
            : String(localized: "Mouse Capture On")
        mouseCaptureIndicatorView.update(message: message)
        bringSubviewToFront(mouseCaptureIndicatorView)
        mouseCaptureIndicatorHideWorkItem?.cancel()
        mouseCaptureIndicatorView.isHidden = false
        UIView.animate(withDuration: TerminalZoomPresentation.indicatorFadeInDuration) {
            self.mouseCaptureIndicatorView.alpha = 1
        }
        UIAccessibility.post(notification: .announcement, argument: message)

        let workItem = DispatchWorkItem { [weak self] in
            guard let self else { return }
            UIView.animate(
                withDuration: TerminalZoomPresentation.indicatorFadeOutDuration,
                animations: {
                    self.mouseCaptureIndicatorView.alpha = 0
                },
                completion: { _ in
                    self.mouseCaptureIndicatorView.isHidden = true
                }
            )
        }
        mouseCaptureIndicatorHideWorkItem = workItem
        DispatchQueue.main.asyncAfter(
            deadline: .now() + TerminalZoomPresentation.indicatorHideDelay,
            execute: workItem
        )
    }
}

final class TerminalMouseCaptureIndicatorView: UIVisualEffectView {
    private let messageLabel = UILabel()

    override init(effect: UIVisualEffect? = UIBlurEffect(style: .systemChromeMaterialDark)) {
        super.init(effect: effect)
        isUserInteractionEnabled = false
        isAccessibilityElement = true
        accessibilityIdentifier = "orlix.terminal.mouseCaptureIndicator"
        clipsToBounds = true
        layer.cornerRadius = 18
        layer.cornerCurve = .continuous

        messageLabel.font = .systemFont(ofSize: 17, weight: .semibold)
        messageLabel.textColor = .white
        messageLabel.textAlignment = .center
        messageLabel.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(messageLabel)

        NSLayoutConstraint.activate([
            messageLabel.leadingAnchor.constraint(greaterThanOrEqualTo: contentView.leadingAnchor, constant: 18),
            messageLabel.trailingAnchor.constraint(lessThanOrEqualTo: contentView.trailingAnchor, constant: -18),
            messageLabel.topAnchor.constraint(greaterThanOrEqualTo: contentView.topAnchor, constant: 12),
            messageLabel.bottomAnchor.constraint(lessThanOrEqualTo: contentView.bottomAnchor, constant: -12),
            messageLabel.centerXAnchor.constraint(equalTo: contentView.centerXAnchor),
            messageLabel.centerYAnchor.constraint(equalTo: contentView.centerYAnchor)
        ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func update(message: String) {
        messageLabel.text = message
        accessibilityLabel = message
    }
}

#endif
