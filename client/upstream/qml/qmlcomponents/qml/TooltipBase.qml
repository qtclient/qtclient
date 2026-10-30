import QtQuick
import QtQuick.Layouts

import qmlcomponents

Item {
    id: root
    signal hoverEnter()
    signal hoverLeave()
    signal mouseButtonPressed()
    signal containsCheck(var point, bool contained)

    property Component delegate: null
    property var calculateDelegatePositionCallback: null
    property alias item: delegateLoader.item
    property point lastHoverPosition: Qt.point(NaN, NaN)
    property bool enabled: true
    property bool mouseContained: false

    Connections {
      id: tooltipHelperConnections
      property bool lastContained: false
      target: null

      function onHoverRefresh() {
        const watcher = target;
        if (!watcher) {
          return;
        }

        const contained =
            root.enabled &&
            watcher.contained(root) &&
            TooltipHelper.inWindow();

        if (!contained) {
          watcher.unregister();
          return;
        }

        root.mouseContained = true;
        root.lastHoverPosition = watcher.hoverPosition(root);

        root.containsCheck(root.lastHoverPosition, true);

        // The external handler may have removed or unregistered the tooltip.
        if (!root ||
            !tooltipHelperConnections ||
            tooltipHelperConnections.target !== watcher) {
          return;
        }

        root.callCaculatePositionCallbackCaller(root.lastHoverPosition, true);
      }

      function onUnregistered() {
        const wasContained = root.mouseContained;

        target = null;
        root.mouseContained = false;

        // Notify external handlers after clearing the internal state.
        if (wasContained) {
          root.hoverLeave();
        }
      }

      function onMouseButtonPressed() {
        root.mouseButtonPressed();
      }
    }

    function callCaculatePositionCallbackCaller(point, contained) {
      const tooltipRoot = root;
      const loader = delegateLoader;

      if (!tooltipRoot || !loader || !loader.item) {
          return;
      }

      const delegateItem = loader.item;
      const positionCallback = tooltipRoot.calculateDelegatePositionCallback;

      if (typeof positionCallback !== "function") {
        return;
      }

      const calculatedPoint =
          positionCallback.call(tooltipRoot, point, contained);

      // The callback may unload the host or replace the delegate.
      if (!tooltipRoot ||
          !loader ||
          !delegateItem ||
          loader.item !== delegateItem) {
        return;
      }

      if (!calculatedPoint ||
          !Number.isFinite(calculatedPoint.x) ||
          !Number.isFinite(calculatedPoint.y)) {
        return;
      }

      const window = TooltipHelper.findWindowContentItemForQuickItem(loader);
      if (!window) {
        return;
      }

      const windowPosition = tooltipRoot.mapToItem(window,
          Qt.point(calculatedPoint.x, calculatedPoint.y));

      if (!Number.isFinite(windowPosition.x) ||
          !Number.isFinite(windowPosition.y)) {
        return;
      }

      if (loader.x !== windowPosition.x) {
        loader.x = windowPosition.x;
      }

      // Changing x may trigger handlers that unload or replace the delegate.
      if (!tooltipRoot ||
          !loader ||
          !delegateItem ||
          loader.item !== delegateItem) {
        return;
      }

      if (loader.y !== windowPosition.y) {
        loader.y = windowPosition.y;
      }
    }

    function showTooltip() {
      let window = TooltipHelper.findWindowContentItemForQuickItem(delegateLoader);
      if (!window) {
        return;
      }
      if (item == null) {
        delegateLoader.sourceComponent = delegate;
        delegateLoader.parent = window;
      }
      callCaculatePositionCallbackCaller(root.lastHoverPosition, true);
    }

    function hideTooltip() {
      if (item != null) {
        delegateLoader.sourceComponent = undefined;
      }
    }

    function recalculatePointToFitDelegateIntoWindow(point) {
      let window = TooltipHelper.findWindowContentItemForQuickItem(delegateLoader);
      if (!window) {
        return point;
      }

      let windowPosition = root.mapToItem(window, point);
      if (delegateLoader.item) {
        let delegateItem = delegateLoader.item;

        var newX = windowPosition.x;
        var newY = windowPosition.y;

        newX = Math.max(0, Math.min(newX, window.width - delegateItem.width));
        newY = Math.max(0, Math.min(newY, window.height - delegateItem.height));

        windowPosition = Qt.point(newX, newY);
      }
      return root.mapFromItem(window, windowPosition);
    }

    onEnabledChanged: {
      if (!root.enabled) {
        const watcher = tooltipHelperConnections.target;

        hideTooltip();

        if (watcher) {
          watcher.unregister();
        }
      }
    }
    
    Loader {
      id: delegateLoader
    }

    MouseArea {
      id: mouseRoot
      anchors.fill: parent
      hoverEnabled: root.enabled
      acceptedButtons: Qt.NoButton 
      preventStealing: true

      onEntered: () => {
        const watcher = TooltipHelper.registerTooltip(root);
        if (!watcher) {
          return;
        }

        tooltipHelperConnections.target = watcher;
        lastHoverPosition = watcher.hoverPosition(root);
        mouseContained = true;

        // External handlers may unload the tooltip host.
        root.hoverEnter();
      }
    }
  }
