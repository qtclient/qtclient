#pragma once

namespace tibia {
namespace reporting {

/// The session's reporting state: whether the server takes bug reports (0)
/// or why it refuses them (1..3); -1 until the server says.
class TReportingStorage
{
public:
    int bugReportState() const { return m_bugReportState; }
    void setBugReportState(const int state) { m_bugReportState = state; }

private:
    int m_bugReportState = -1;
};

}
}
