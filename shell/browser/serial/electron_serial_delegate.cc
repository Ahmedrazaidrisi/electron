// Copyright (c) 2020 Microsoft, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/serial/electron_serial_delegate.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "content/public/browser/web_contents.h"
#include "shell/browser/api/electron_api_web_contents.h"
#include "shell/browser/serial/serial_chooser.h"
#include "shell/browser/serial/serial_chooser_context.h"
#include "shell/browser/serial/serial_chooser_context_factory.h"
#include "shell/browser/serial/serial_chooser_controller.h"
#include "shell/browser/web_contents_permission_helper.h"

namespace features {

const base::Feature kElectronSerialChooser{"ElectronSerialChooser",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
}

namespace electron {

SerialChooserContext* GetChooserContext(content::RenderFrameHost* frame) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* browser_context = web_contents->GetBrowserContext();
  return SerialChooserContextFactory::GetForBrowserContext(browser_context);
}

ElectronSerialDelegate::ElectronSerialDelegate() = default;

ElectronSerialDelegate::~ElectronSerialDelegate() {
  LOG(INFO) << "IN ~ElectronSerialDelegate()";
}

std::unique_ptr<content::SerialChooser> ElectronSerialDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback) {
  if (base::FeatureList::IsEnabled(features::kElectronSerialChooser)) {
    SerialChooserController* controller = ControllerForFrame(frame);
    if (controller) {
      DeleteControllerForFrame(frame);
    }
    AddControllerForFrame(frame, std::move(filters), std::move(callback));
  } else {
    // If feature is disabled, immediately return back with no port selected.
    std::move(callback).Run(nullptr);
  }
  return std::make_unique<SerialChooser>(base::DoNothing());
}

bool ElectronSerialDelegate::CanRequestPortPermission(
    content::RenderFrameHost* frame) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* permission_helper =
      WebContentsPermissionHelper::FromWebContents(web_contents);
  return permission_helper->CheckSerialAccessPermission(
      web_contents->GetMainFrame()->GetLastCommittedOrigin());
}

bool ElectronSerialDelegate::HasPortPermission(
    content::RenderFrameHost* frame,
    const device::mojom::SerialPortInfo& port) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* browser_context = web_contents->GetBrowserContext();
  auto* chooser_context =
      SerialChooserContextFactory::GetForBrowserContext(browser_context);
  return chooser_context->HasPortPermission(
      frame->GetLastCommittedOrigin(),
      web_contents->GetMainFrame()->GetLastCommittedOrigin(), port);
}

device::mojom::SerialPortManager* ElectronSerialDelegate::GetPortManager(
    content::RenderFrameHost* frame) {
  return GetChooserContext(frame)->GetPortManager();
}

void ElectronSerialDelegate::AddObserver(content::RenderFrameHost* frame,
                                         Observer* observer) {
  return GetChooserContext(frame)->AddPortObserver(observer);
}

void ElectronSerialDelegate::RemoveObserver(content::RenderFrameHost* frame,
                                            Observer* observer) {
  LOG(INFO) << "In ElectronSerialDelegate::RemoveObserver";
  SerialChooserContext* serial_chooser_context = GetChooserContext(frame);
  if (serial_chooser_context) {
    return serial_chooser_context->RemovePortObserver(observer);
  }
}

SerialChooserController* ElectronSerialDelegate::ControllerForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = controller_map_.find(render_frame_host);
  return mapping == controller_map_.end() ? nullptr : mapping->second.get();
}

SerialChooserController* ElectronSerialDelegate::AddControllerForFrame(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto controller = std::make_unique<SerialChooserController>(
      render_frame_host, std::move(filters), std::move(callback), web_contents,
      weak_factory_.GetWeakPtr());
  controller_map_.insert(
      std::make_pair(render_frame_host, std::move(controller)));
  return ControllerForFrame(render_frame_host);
}

void ElectronSerialDelegate::DeleteControllerForFrame(
    content::RenderFrameHost* render_frame_host) {
  controller_map_.erase(render_frame_host);
}

}  // namespace electron
