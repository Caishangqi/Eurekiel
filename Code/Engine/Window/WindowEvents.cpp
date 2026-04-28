#include "WindowEvents.hpp"

namespace enigma::window
{
    enigma::event::MulticastDelegate<const WindowClientSizeEvent&> WindowEvents::OnClientSizeChanged;
    enigma::event::MulticastDelegate<const WindowModeChangedEvent&> WindowEvents::OnWindowModeChanged;
    enigma::event::MulticastDelegate<const WindowCloseRequest&> WindowEvents::OnWindowCloseRequested;

    enigma::event::MulticastDelegate<const WindowModeRequest&> WindowModeRequestEvents::OnWindowModeRequested;
} // namespace enigma::window
